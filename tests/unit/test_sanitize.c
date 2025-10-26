/*
 * Unit tests for sanitize_message() function
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Copy of sanitize_message() function for testing */
void
sanitize_message(char *s)
{
	char *p;
	char *start;
	int   len;

	/* Replace embedded newlines and control chars with spaces */
	for (p = s; *p != '\0'; p++) {
		if (*p == '\n' || *p == '\r' || (*p < 32 && *p != '\t')) {
			*p = ' ';
		}
	}

	/* Trim leading spaces */
	start = s;
	while (isspace(*start))
		start++;

	/* Shift string left if needed */
	if (start != s) {
		p = s;
		while (*start != '\0')
			*p++ = *start++;
		*p = '\0';
	}

	/* Trim trailing spaces */
	len = strlen(s);
	while (len > 0 && isspace(s[len - 1]))
		len--;
	s[len] = '\0';
}

void
test_sanitize_basic()
{
	char str[] = "hello world";
	sanitize_message(str);

	assert(strcmp(str, "hello world") == 0);
	printf("[PASS] test_sanitize_basic\n");
}

void
test_sanitize_leading_spaces()
{
	char str[] = "   hello";
	sanitize_message(str);

	assert(strcmp(str, "hello") == 0);
	printf("[PASS] test_sanitize_leading_spaces\n");
}

void
test_sanitize_trailing_spaces()
{
	char str[] = "hello   ";
	sanitize_message(str);

	assert(strcmp(str, "hello") == 0);
	printf("[PASS] test_sanitize_trailing_spaces\n");
}

void
test_sanitize_both_spaces()
{
	char str[] = "   hello   ";
	sanitize_message(str);

	assert(strcmp(str, "hello") == 0);
	printf("[PASS] test_sanitize_both_spaces\n");
}

void
test_sanitize_only_spaces()
{
	char str[] = "     ";
	sanitize_message(str);

	assert(strcmp(str, "") == 0);
	printf("[PASS] test_sanitize_only_spaces\n");
}

void
test_sanitize_empty_string()
{
	char str[] = "";
	sanitize_message(str);

	assert(strcmp(str, "") == 0);
	printf("[PASS] test_sanitize_empty_string\n");
}

void
test_sanitize_embedded_newlines()
{
	char str[] = "hello\nworld\n";
	sanitize_message(str);

	assert(strcmp(str, "hello world") == 0);
	printf("[PASS] test_sanitize_embedded_newlines\n");
}

void
test_sanitize_embedded_carriage_returns()
{
	char str[] = "hello\r\nworld\r\n";
	sanitize_message(str);

	/* \r\n becomes two spaces, trailing spaces trimmed */
	assert(strcmp(str, "hello   world") == 0 || strcmp(str, "hello  world") == 0);
	assert(strchr(str, '\r') == NULL);
	assert(strchr(str, '\n') == NULL);
	printf("[PASS] test_sanitize_embedded_carriage_returns\n");
}

void
test_sanitize_multiline_sip_message()
{
	char str[] = "INVITE sip:user@example.com SIP/2.0\r\nVia: SIP/2.0/UDP test\r\n";
	sanitize_message(str);

	/* Newlines should be replaced with spaces, trailing spaces trimmed */
	assert(strstr(str, "INVITE sip:user@example.com SIP/2.0") != NULL);
	assert(strchr(str, '\n') == NULL);
	assert(strchr(str, '\r') == NULL);
	printf("[PASS] test_sanitize_multiline_sip_message\n");
}

void
test_sanitize_control_characters()
{
	char str[256];

	/* Build string with control characters */
	strcpy(str, "hello");
	str[5] = 0x01; /* SOH */
	str[6] = 0x02; /* STX */
	str[7] = 0x1F; /* Unit separator */
	strcpy(str + 8, "world");

	sanitize_message(str);

	/* Control chars should be replaced with spaces */
	assert(strchr(str, 0x01) == NULL);
	assert(strchr(str, 0x02) == NULL);
	assert(strchr(str, 0x1F) == NULL);
	assert(strstr(str, "hello") != NULL);
	assert(strstr(str, "world") != NULL);
	printf("[PASS] test_sanitize_control_characters\n");
}

void
test_sanitize_preserves_tabs()
{
	char str[] = "hello\tworld\t";
	sanitize_message(str);

	/* Tabs should be preserved in the middle, trimmed at end */
	assert(strchr(str, '\t') != NULL);
	assert(strcmp(str, "hello\tworld") == 0);
	printf("[PASS] test_sanitize_preserves_tabs\n");
}

void
test_sanitize_only_newlines()
{
	char str[] = "\n\n\n\n";
	sanitize_message(str);

	assert(strcmp(str, "") == 0);
	printf("[PASS] test_sanitize_only_newlines\n");
}

void
test_sanitize_mixed_whitespace()
{
	char str[] = "  \n\t hello \n world \r\n  ";
	sanitize_message(str);

	/* Should have no leading/trailing whitespace, newlines replaced */
	assert(str[0] != ' ' && str[0] != '\n' && str[0] != '\t');
	assert(strchr(str, '\n') == NULL);
	assert(strchr(str, '\r') == NULL);
	printf("[PASS] test_sanitize_mixed_whitespace\n");
}

void
test_sanitize_csv_safety()
{
	char str[] = "field1\nfield2,field3\rfield4";
	sanitize_message(str);

	/* Should not contain newlines that would break CSV */
	assert(strchr(str, '\n') == NULL);
	assert(strchr(str, '\r') == NULL);
	/* Comma should be preserved */
	assert(strchr(str, ',') != NULL);
	printf("[PASS] test_sanitize_csv_safety\n");
}

void
test_sanitize_fuzz_random_strings()
{
	const int num_tests = 1000;
	const int max_len = 256;
	char *test_str;
	int i, j;

	printf("[INFO] Running fuzz test with %d random strings...\n", num_tests);

	for (i = 0; i < num_tests; i++) {
		size_t len = rand() % max_len;
		test_str = malloc(len + 1);
		assert(test_str != NULL);

		/* Generate random string with printable and control chars */
		for (j = 0; j < (int)len; j++) {
			test_str[j] = rand() % 128;
		}
		test_str[len] = '\0';

		sanitize_message(test_str);

		/* After sanitization, should have no newlines or dangerous control chars */
		assert(strchr(test_str, '\n') == NULL);
		assert(strchr(test_str, '\r') == NULL);

		free(test_str);
	}

	printf("[PASS] test_sanitize_fuzz_random_strings (%d iterations)\n", num_tests);
}

void
test_sanitize_fuzz_multiline_patterns()
{
	const int num_tests = 100;
	const int max_len = 512;
	char *test_str;
	int i, j;

	printf("[INFO] Fuzzing with multiline patterns...\n");

	for (i = 0; i < num_tests; i++) {
		size_t len = (rand() % max_len) + 1;
		test_str = malloc(len + 1);
		assert(test_str != NULL);

		/* Generate strings with varying newline patterns */
		for (j = 0; j < (int)len; j++) {
			int r = rand() % 10;
			if (r < 2) {
				test_str[j] = '\n';
			} else if (r == 2) {
				test_str[j] = '\r';
			} else if (r == 3) {
				test_str[j] = '\t';
			} else {
				test_str[j] = 'a' + (rand() % 26);
			}
		}
		test_str[len] = '\0';

		sanitize_message(test_str);

		/* No newlines should remain */
		assert(strchr(test_str, '\n') == NULL);
		assert(strchr(test_str, '\r') == NULL);

		free(test_str);
	}

	printf("[PASS] test_sanitize_fuzz_multiline_patterns (%d iterations)\n", num_tests);
}

void
test_sanitize_very_long_strings()
{
	const size_t sizes[] = {1024, 4096, 8192};
	char *test_str;
	int i;

	printf("[INFO] Testing very long strings...\n");

	for (i = 0; i < 3; i++) {
		size_t size = sizes[i];
		test_str = malloc(size + 1);
		assert(test_str != NULL);

		memset(test_str, 'a', size);
		test_str[size] = '\0';

		sanitize_message(test_str);

		assert(strlen(test_str) == size);

		free(test_str);
	}

	printf("[PASS] test_sanitize_very_long_strings\n");
}

int
main()
{
	printf("Running sanitize_message() unit tests...\n\n");

	/* Seed random number generator */
	srand(time(NULL));

	/* Basic tests */
	test_sanitize_basic();
	test_sanitize_leading_spaces();
	test_sanitize_trailing_spaces();
	test_sanitize_both_spaces();
	test_sanitize_only_spaces();
	test_sanitize_empty_string();

	printf("\n");

	/* Sanitization-specific tests */
	test_sanitize_embedded_newlines();
	test_sanitize_embedded_carriage_returns();
	test_sanitize_multiline_sip_message();
	test_sanitize_control_characters();
	test_sanitize_preserves_tabs();
	test_sanitize_only_newlines();
	test_sanitize_mixed_whitespace();
	test_sanitize_csv_safety();

	printf("\n");

	/* Fuzz tests */
	test_sanitize_fuzz_random_strings();
	test_sanitize_fuzz_multiline_patterns();
	test_sanitize_very_long_strings();

	printf("\nAll tests passed!\n");
	return 0;
}
