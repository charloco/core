/* Copyright (c) 2009-2016 Dovecot authors, see the included COPYING file */

#include "test-lib.h"
#include "str.h"
#include "env-util.h"
#include "hostpid.h"
#include "var-expand.h"

struct var_expand_test {
	const char *in;
	const char *out;
	int ret;
};

struct var_get_key_range_test {
	const char *in;
	unsigned int idx, size;
};

static void test_var_expand_ranges(void)
{
	static struct var_expand_test tests[] = {
		{ "%v", "value1234", 1 },
		{ "%3v", "val", 1 },
		{ "%3.2v", "ue", 1 },
		{ "%3.-2v", "ue12", 1 },
		{ "%-3.2v", "23", 1 },
		{ "%0.-1v", "value123", 1 },
		{ "%-4.-1v", "123", 1 }
	};
	static struct var_expand_table table[] = {
		{ 'v', "value1234", NULL },
		{ '\0', NULL, NULL }
	};
	string_t *str = t_str_new(128);
	const char *error;
	unsigned int i;

	test_begin("var_expand - ranges");
	for (i = 0; i < N_ELEMENTS(tests); i++) {
		str_truncate(str, 0);
		test_assert(var_expand(str, tests[i].in, table, &error) == tests[i].ret);
		test_assert(strcmp(tests[i].out, str_c(str)) == 0);
	}
	test_end();
}

static void test_var_expand_builtin(void)
{
	static struct var_expand_test tests[] = {
		{ "%{hostname}", NULL, 1 },
		{ "%{pid}", NULL, 1 },
		{ "a%{env:FOO}b", "abaRb", 1 },
		{ "%50Hv", "1f", 1 },
		{ "%50Hw", "2e", 1 },
		{ "%50Nv", "25", 1 },
		{ "%50Nw", "e", 1 },

		{ "%{nonexistent}", "UNSUPPORTED_VARIABLE_nonexistent", 0 },
		{ "%{nonexistent:default}", "UNSUPPORTED_VARIABLE_nonexistent", 0 },
	};
	static struct var_expand_table table[] = {
		{ 'v', "value", NULL },
		{ 'w', "value2", NULL },
		{ '\0', NULL, NULL }
	};
	string_t *str = t_str_new(128);
	const char *error;
	unsigned int i;

	tests[0].out = my_hostname;
	tests[1].out = my_pid;
	env_put("FOO=baR");

	test_begin("var_expand - builtin");
	for (i = 0; i < N_ELEMENTS(tests); i++) {
		str_truncate(str, 0);
		test_assert_idx(var_expand(str, tests[i].in, table, &error) == tests[i].ret, i);
		test_assert_idx(strcmp(tests[i].out, str_c(str)) == 0, i);
	}
	test_end();
}

static void test_var_get_key_range(void)
{
	static struct var_get_key_range_test tests[] = {
		{ "", 0, 0 },
		{ "{", 1, 0 },
		{ "k", 0, 1 },
		{ "{key}", 1, 3 },
		{ "5.5Rk", 4, 1 },
		{ "5.5R{key}", 5, 3 },
		{ "{key", 1, 3 }
	};
	unsigned int i, idx, size;

	test_begin("var_get_key_range");
	for (i = 0; i < N_ELEMENTS(tests); i++) {
		var_get_key_range(tests[i].in, &idx, &size);
		test_assert_idx(tests[i].idx == idx, i);
		test_assert_idx(tests[i].size == size, i);

		if (tests[i].size == 1)
			test_assert_idx(tests[i].in[idx] == var_get_key(tests[i].in), i);
	}
	test_end();
}

static int test_var_expand_func1(const char *data, void *context,
				 const char **value_r,
				 const char **error_r ATTR_UNUSED)
{
	test_assert(*(int *)context == 0xabcdef);
	*value_r = t_strdup_printf("<%s>", data);
	return 1;
}

static int test_var_expand_func2(const char *data ATTR_UNUSED,
				 void *context ATTR_UNUSED,
				 const char **value_r,
				 const char **error_r ATTR_UNUSED)
{
	*value_r = "";
	return 1;
}

static int test_var_expand_func3(const char *data ATTR_UNUSED,
				 void *context ATTR_UNUSED,
				 const char **value_r,
				 const char **error_r ATTR_UNUSED)
{
	*value_r = NULL;
	return 1;
}

static int test_var_expand_func4(const char *data,
				 void *context ATTR_UNUSED,
				 const char **value_r ATTR_UNUSED,
				 const char **error_r)
{
	*error_r = t_strdup_printf("Unknown data %s", data == NULL ? "" : data);
	return 0;
}

static int test_var_expand_func5(const char *data ATTR_UNUSED,
				 void *context ATTR_UNUSED,
				 const char **value_r ATTR_UNUSED,
				 const char **error_r)
{
	*error_r = "Internal error";
	return -1;
}

static void test_var_expand_with_funcs(void)
{
	static struct var_expand_test tests[] = {
		{ "%{func1}", "<>", 1 },
		{ "%{func1:foo}", "<foo>", 1 },
		{ "%{func2}", "", 1 },
		{ "%{func3}", "", 1 },
		{ "%{func4}", "", 0 },
		{ "%{func5}", "", -1 },
		{ "%{func4}%{func5}", "", -1 },
		{ "%{func5}%{func4}%{func3}", "", -1 },
	};
	static struct var_expand_table table[] = {
		{ '\0', NULL, NULL }
	};
	static const struct var_expand_func_table func_table[] = {
		{ "func1", test_var_expand_func1 },
		{ "func2", test_var_expand_func2 },
		{ "func3", test_var_expand_func3 },
		{ "func4", test_var_expand_func4 },
		{ "func5", test_var_expand_func5 },
		{ NULL, NULL }
	};
	string_t *str = t_str_new(128);
	const char *error;
	unsigned int i;
	int ctx = 0xabcdef;

	test_begin("var_expand_with_funcs");
	for (i = 0; i < N_ELEMENTS(tests); i++) {
		str_truncate(str, 0);
		test_assert_idx(var_expand_with_funcs(str, tests[i].in, table, func_table, &ctx, &error) == tests[i].ret, i);
		test_assert_idx(strcmp(tests[i].out, str_c(str)) == 0, i);
	}
	test_end();
}

static void test_var_get_key(void)
{
	static struct {
		const char *str;
		char key;
	} tests[] = {
		{ "x", 'x' },
		{ "2.5Mx", 'x' },
		{ "200MDx", 'x' },
		{ "200MD{foo}", '{' },
		{ "{foo}", '{' },
		{ "", '\0' },
	};

	test_begin("var_get_key");
	for (unsigned int i = 0; i < N_ELEMENTS(tests); i++)
		test_assert_idx(var_get_key(tests[i].str) == tests[i].key, i);
	test_end();
}

static void test_var_has_key(void)
{
	static struct {
		const char *str;
		char key;
		const char *long_key;
		bool result;
	} tests[] = {
		{ "%x%y", 'x', NULL, TRUE },
		{ "%x%y", 'y', NULL, TRUE },
		{ "%x%y", 'z', NULL, FALSE },
		{ "%{foo}", 'f', NULL, FALSE },
		{ "%{foo}", 'o', NULL, FALSE },
		{ "%{foo}", '\0', "foo", TRUE },
		{ "%{foo}", 'o', "foo", TRUE },
		{ "%2.5Mx%y", 'x', NULL, TRUE },
		{ "%2.5M{foo}", '\0', "foo", TRUE },
	};

	test_begin("var_has_key");
	for (unsigned int i = 0; i < N_ELEMENTS(tests); i++)
		test_assert_idx(var_has_key(tests[i].str, tests[i].key, tests[i].long_key) == tests[i].result, i);
	test_end();
}

static int test_var_expand_hashing_func1(const char *data,
					 void *context ATTR_UNUSED,
					 const char **value_r,
					 const char **error_r ATTR_UNUSED)
{
	*value_r = data;
	return 1;
}

static void test_var_expand_hashing(void)
{
	const char *error;
	test_begin("var_expand_hashing");

	static struct var_expand_table table[] = {
		{'\0', "example", "value" },
		{'\0', "other-example", "other-value" },
		{'\0', NULL, NULL}
	};

	static struct {
		const char *in;
		const char *out;
	} tests[] = {
		{ "md5: %M{value} %{md5:value}", "md5: 1a79a4d60de6718e8e5b326e338ae533 1a79a4d60de6718e8e5b326e338ae533" },
		{ "sha1: %{sha1:value}", "sha1: c3499c2729730a7f807efb8676a92dcb6f8a3f8f" },
		{ "sha1: %{sha1:func1:example}", "sha1: c3499c2729730a7f807efb8676a92dcb6f8a3f8f" },
		{ "truncate: %{sha1;truncate=12:value}", "truncate: 0c34" },
		{ "truncate: %{sha1;truncate=16:value}", "truncate: c349" },
		{ "rounds,salt: %{sha1;rounds=1000,salt=seawater:value}", "rounds,salt: b515c85884f6b82dc7588279f3643a73e55d2289" },
		{ "rounds,salt,expand: %{sha1;rounds=1000,salt=%{other-value}:value} %{other-value}", "rounds,salt,expand: 49a598ee110af615e175f2e4511cc5d7ccff96ab other-example" },
		{ "format: %4.8{sha1:value}", "format: 9c272973" },
		{ "base64: %{sha1;format=base64:value}", "base64: w0mcJylzCn+AfvuGdqkty2+KP48=" },
	};

	static const struct var_expand_func_table func_table[] = {
		{ "func1", test_var_expand_hashing_func1 },
		{ NULL, NULL }
	};

	string_t *str = t_str_new(128);

	for (unsigned int i = 0; i < N_ELEMENTS(tests); i++) {
		str_truncate(str, 0);
		test_assert(var_expand_with_funcs(str, tests[i].in, table,
			    func_table, NULL, &error) == 1);
		test_assert_idx(strcmp(str_c(str), tests[i].out) == 0, i);
	}

	test_end();
}

void test_var_expand(void)
{
	test_var_expand_ranges();
	test_var_expand_builtin();
	test_var_get_key_range();
	test_var_expand_with_funcs();
	test_var_get_key();
	test_var_has_key();
	test_var_expand_hashing();
}
