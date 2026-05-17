/*
 * Test harness for normalize_path() function
 * Compile: gcc -o test_normalize_path test_normalize_path.c -fsanitize=address -g -Wall
 * Run: ./test_normalize_path
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>
#include <unistd.h>

#define PROGRAM_NAME "test_normalize_path"
#define _(x) x

/* ============ Mock/Stub Functions ============ */

/* Global mock CWD buffer */
static char mock_cwd_buffer[8192] = "/home/user";
static int use_mock_cwd = 0;

/* Stub: tilde_expand - just return NULL to skip tilde expansion */
char *tilde_expand(char *str) {
    (void)str;
    return NULL;
}

/* Stub: unescape_str - just return NULL to skip unescaping */
char *unescape_str(const char *str) {
    (void)str;
    return NULL;
}

/* Mock: get_cwd - return controlled values for testing */
const char *get_cwd(char *buf, const size_t buflen, const int check_workspace) {
    (void)check_workspace;
    if (use_mock_cwd) {
        strncpy(buf, mock_cwd_buffer, buflen);
        buf[buflen - 1] = '\0';
        return buf;
    }
    const char *ret = getcwd(buf, buflen);
    return (ret && *buf) ? buf : NULL;
}

/* ============ Helper Functions ============ */

#if defined(__has_builtin) && !defined(NO_BUILTIN_MUL_OVERFLOW)
# if __has_builtin(__builtin_mul_overflow)
#  define HAVE_BUILTIN_MUL_OVERFLOW
# endif
#endif

size_t xstrsncpy(char *restrict dst, const char *restrict src, size_t n) {
    const char *end = memccpy(dst, src, '\0', n);
    if (!end) {
        dst[n - 1] = '\0';
        end = dst + n;
    }
    return (size_t)(end - dst);
}

void *xmemrchr(const void *s, const int c, size_t n) {
#ifdef HAVE_MEMRCHR
    return memrchr(s, c, n);
#else
    if (n != 0) {
        const unsigned char *cp = (const unsigned char *)s + n;
        do {
            if (*(--cp) == (const unsigned char)c)
                return((void *)cp);
        } while (--n != 0);
    }
    return NULL;
#endif
}

void *xnmalloc(const size_t nmemb, const size_t size) {
#ifdef HAVE_BUILTIN_MUL_OVERFLOW
    size_t r;
    if (__builtin_mul_overflow(nmemb, size, &r))
        r = SIZE_MAX;
    void *p = r == SIZE_MAX ? NULL : malloc(r);
#else
    void *p = malloc(nmemb * size);
#endif

    if (!p) {
        fprintf(stderr, "%s: %s failed to allocate %zux%zu bytes\n",
            PROGRAM_NAME, __func__, nmemb, size);
        /* For testing: return NULL instead of exit to allow graceful handling */
        return NULL;
    }
    return p;
}

void *xnrealloc(void *ptr, const size_t nmemb, const size_t size) {
#ifdef HAVE_BUILTIN_MUL_OVERFLOW
    size_t r;
    if (__builtin_mul_overflow(nmemb, size, &r))
        r = SIZE_MAX;
    void *p = r == SIZE_MAX ? NULL : realloc(ptr, r);
#else
    void *p = realloc(ptr, nmemb * size);
#endif

    if (!p) {
        fprintf(stderr, "%s: %s failed to allocate %zux%zu bytes\n",
            PROGRAM_NAME, __func__, nmemb, size);
        /* For testing: return NULL instead of exit to allow graceful handling */
        return NULL;
    }
    return p;
}

/* ============ Function Under Test ============ */

char *
normalize_path(char *src, const size_t src_len)
{
    if (!src || !*src)
        return NULL;

    /* Unescape SRC */
    char *tmp = NULL;
    const int is_escaped = *src == '\\';

    if (strchr(src, '\\')) {
        tmp = unescape_str(src);
        if (!tmp) {
            fprintf(stderr, "%s: '%s': Error unescaping string\n", PROGRAM_NAME, src);
            return NULL;
        }

        const size_t tlen = strlen(tmp);
        if (tlen > 1 && tmp[tlen - 1] == '/')
            tmp[tlen - 1] = '\0';

        xstrsncpy(src, tmp, tlen + 1);
        free(tmp);
        tmp = NULL;
    }

    /* Expand tilde */
    if (is_escaped == 0 && *src == '~') {
        tmp = tilde_expand(src);
        if (!tmp) {
            fprintf(stderr, "%s: '%s': Error expanding tilde\n", PROGRAM_NAME, src);
            return NULL;
        }
        const size_t tlen = strlen(tmp);
        if (tlen > 1 && tmp[tlen - 1] == '/')
            tmp[tlen - 1] = '\0';

        if (!strstr(tmp, "/.."))
            return tmp;
    }

    char *s = tmp ? tmp : src;
    const size_t l = tmp ? strlen(tmp) : src_len;

    /* Resolve references to . and .. */
    char *res = NULL;
    size_t res_len = 0;
    size_t buf_size = 0;

    if (l == 0 || *s != '/') {
        /* Relative path */
        char p[PATH_MAX + 1]; *p = '\0';
        const char *cwd = get_cwd(p, sizeof(p), 1);
        if (!cwd || !*cwd) {
            fprintf(stderr, "%s: Error getting current directory\n", PROGRAM_NAME);
            free(tmp);
            return NULL;
        }

        const size_t pwd_len = strlen(cwd);
        if (pwd_len == 1 && *cwd == '/') {
            /* If CWD is root (/) do not copy anything. Just create a buffer
             * big enough to hold "/dir", which will be appended next */
            buf_size = l + 2;
            res = xnmalloc(buf_size, sizeof(char));
            res_len = 0;
        } else {
            buf_size = pwd_len + 1 + l + 1;
            res = xnmalloc(buf_size, sizeof(char));
            if (pwd_len >= buf_size) { free(tmp); free(res); return NULL; }
            memcpy(res, cwd, pwd_len);
            res_len = pwd_len;
        }
    } else {
        buf_size = l + 1;
        res = xnmalloc(buf_size, sizeof(char));
        res_len = 0;
    }

    const char *ptr;
    const char *end = &s[l];
    const char *next;

    for (ptr = s; ptr < end; ptr = next + 1) {
        next = memchr(ptr, '/', (size_t)(end - ptr));
        if (!next)
            next = end;
        const size_t len = (size_t)(next - ptr);

        if (len <= 2) {
            if (len == 0) continue;
            if (len == 1 && *ptr == '.') continue;
            if (len == 2 && *ptr == '.' && ptr[1] == '.') {
                const char *slash = xmemrchr(res, '/', res_len);
                if (slash)
                    res_len = (size_t)(slash - res);
                continue;
            }
        }

        if (res_len + 1 + len >= buf_size) { free(res); if (s == tmp) free(s); return NULL; }
        res[res_len++] = '/';
        memcpy(&res[res_len], ptr, len);
        res_len += len;
    }

    if (res_len == 0)
        res[res_len++] = '/';


    res[res_len] = '\0';

    if (res_len > 1 && res[res_len - 1] == '/')
        res[res_len - 1] = '\0';

    if (s == tmp)
        free(s);

    return res;
}

/* ============ Test Cases ============ */

void test_1_long_cwd_and_long_relative_path(void) {
    printf("\n=== Test 1: Long CWD + Long Relative Path ===\n");

    /* Create a long CWD path (2000 chars) */
    use_mock_cwd = 1;
    memset(mock_cwd_buffer, 0, sizeof(mock_cwd_buffer));
    strcpy(mock_cwd_buffer, "/");
    for (int i = 0; i < 100; i++) {
        strcat(mock_cwd_buffer, "very_long_dir_name/");
    }
    /* Remove trailing slash */
    mock_cwd_buffer[strlen(mock_cwd_buffer) - 1] = '\0';
    printf("CWD length: %zu\n", strlen(mock_cwd_buffer));

    /* Create a long relative path (1000 chars) */
    char long_path[1536];
    strcpy(long_path, "subdir/");
    for (int i = 0; i < 50; i++) {
        strcat(long_path, "another_long_dir/");
    }
    strcat(long_path, "file.txt");
    printf("Relative path length: %zu\n", strlen(long_path));
    printf("Total would be: %zu\n", strlen(mock_cwd_buffer) + 1 + strlen(long_path));

    char *result = normalize_path(long_path, strlen(long_path));
    if (result) {
        printf("Result length: %zu\n", strlen(result));
        printf("Result (first 80 chars): %.80s...\n", result);
        free(result);
        printf("✓ Test passed (no crash)\n");
    } else {
        printf("✗ Result is NULL (allocation failed or error)\n");
    }

    use_mock_cwd = 0;
}

void test_2_many_dotdot_with_expansion(void) {
    printf("\n=== Test 2: Many .. Components with Expansion ===\n");

    use_mock_cwd = 1;
    strcpy(mock_cwd_buffer, "/a/b/c/d/e");
    printf("CWD: %s\n", mock_cwd_buffer);

    /* Backtrack with many .., then expand */
    char path[2048] = "../../../../../../../../";
    for (int i = 0; i < 50; i++) {
        strcat(path, "new_very_long_directory_segment/");
    }
    strcat(path, "file.txt");
    printf("Path length: %zu\n", strlen(path));

    char *result = normalize_path(path, strlen(path));
    if (result) {
        printf("Result: %s\n", strlen(result) > 100 ? "(truncated)" : result);
        printf("Result length: %zu\n", strlen(result));
        free(result);
        printf("✓ Test passed (no crash)\n");
    } else {
        printf("✗ Result is NULL\n");
    }

    use_mock_cwd = 0;
}

void test_3_max_path_length(void) {
    printf("\n=== Test 3: Maximum Path Length Test ===\n");

    use_mock_cwd = 1;

    /* Create CWD that's PATH_MAX - 200 chars */
    memset(mock_cwd_buffer, 0, sizeof(mock_cwd_buffer));
    strcpy(mock_cwd_buffer, "/");
    size_t target_cwd_len = PATH_MAX - 200;
    while (strlen(mock_cwd_buffer) < target_cwd_len - 20) {
        strcat(mock_cwd_buffer, "longdirname/");
    }
    mock_cwd_buffer[strlen(mock_cwd_buffer) - 1] = '\0'; /* Remove trailing slash */
    printf("CWD length: %zu\n", strlen(mock_cwd_buffer));

    /* Create relative path that's 400 chars */
    char path[512];
    strcpy(path, "");
    for (int i = 0; i < 20; i++) {
        strcat(path, "morelongdirname/");
    }
    strcat(path, "file.txt");
    printf("Relative path length: %zu\n", strlen(path));
    printf("Combined would be: %zu (PATH_MAX=%d)\n",
           strlen(mock_cwd_buffer) + 1 + strlen(path), PATH_MAX);

    char *result = normalize_path(path, strlen(path));
    if (result) {
        printf("Result length: %zu\n", strlen(result));
        free(result);
        printf("✓ Test passed (no crash)\n");
    } else {
        printf("✗ Result is NULL\n");
    }

    use_mock_cwd = 0;
}

void test_4_empty_and_special_segments(void) {
    printf("\n=== Test 4: Empty and Special Segments ===\n");

    use_mock_cwd = 1;
    strcpy(mock_cwd_buffer, "/home/user");

    char path[] = "///a///b///..///c///";
    printf("Path: %s\n", path);

    char *result = normalize_path(path, strlen(path));
    if (result) {
        printf("Result: %s\n", result);
        free(result);
        printf("✓ Test passed (no crash)\n");
    } else {
        printf("✗ Result is NULL\n");
    }

    use_mock_cwd = 0;
}

void test_5_root_cwd_edge_case(void) {
    printf("\n=== Test 5: Root CWD with Long Relative Path ===\n");

    use_mock_cwd = 1;
    strcpy(mock_cwd_buffer, "/");
    printf("CWD: %s\n", mock_cwd_buffer);

    /* Create a long relative path (approaching PATH_MAX) */
    char long_path[PATH_MAX];
    strcpy(long_path, "");
    /* Create path that's about 3000 chars */
    for (int i = 0; i < 150 && strlen(long_path) < PATH_MAX - 100; i++) {
        strcat(long_path, "dir/");
    }
    strcat(long_path, "file.txt");
    printf("Relative path length: %zu\n", strlen(long_path));

    char *result = normalize_path(long_path, strlen(long_path));
    if (result) {
        printf("Result length: %zu\n", strlen(result));
        printf("Result (first 60 chars): %.60s...\n", result);
        free(result);
        printf("✓ Test passed (no crash)\n");
    } else {
        printf("✗ Result is NULL (allocation failed or error)\n");
    }

    use_mock_cwd = 0;
}

void test_6_trigger_line_575_check(void) {
    printf("\n=== Test 6: Try to Trigger Line 575 Buffer Check ===\n");
    printf("Attempting to create a scenario where res_len + 1 + len >= buf_size\n");

    /* The line 575 check triggers when:
     * res_len + 1 + len >= buf_size
     *
     * This happens if:
     * - Buffer is allocated for pwd_len + l + 2
     * - But after backtracking with .., we have less content in res
     * - Then we try to append something that would exceed the original allocation
     *
     * Example:
     * CWD: "/a/b/c/d/e" (len 10)
     * Path: "../../../../x/y/z/..." (long expansion after backtracking)
     * Buffer allocated for: 10 + 1 + path_len + 1
     * But after "..", res only has "/"
     * Then trying to append many long segments might exceed
     */

    use_mock_cwd = 1;
    strcpy(mock_cwd_buffer, "/short");
    printf("CWD: %s (len %zu)\n", mock_cwd_buffer, strlen(mock_cwd_buffer));

    /* Path: backtrack completely, then add many long segments */
    char path[2048];
    strcpy(path, "../..");  /* Backtrack to root */
    for (int i = 0; i < 50; i++) {
        strcat(path, "/very_long_directory_name_here");
    }
    printf("Path length: %zu\n", strlen(path));
    printf("Initial buffer would be: pwd_len(%zu) + 1 + l(%zu) + 1 = %zu\n",
           strlen(mock_cwd_buffer), strlen(path),
           strlen(mock_cwd_buffer) + 1 + strlen(path) + 1);

    char *result = normalize_path(path, strlen(path));
    if (result) {
        printf("Result length: %zu\n", strlen(result));
        printf("Result (first 60 chars): %.60s...\n", result);
        free(result);
        printf("✓ Test passed (no crash)\n");
    } else {
        printf("✗ Result is NULL (may have hit line 575 check and returned NULL)\n");
    }

    use_mock_cwd = 0;
}

void test_7_artificial_overflow(void) {
    printf("\n=== Test 7: Artificial Overflow Analysis ===\n");
    printf("This test analyzes integer overflow in buffer size calculation.\n");

    /* Try to create a scenario where pwd_len + l + 2 could overflow.
     * On 64-bit: SIZE_MAX = 18446744073709551615
     * On 32-bit: SIZE_MAX = 4294967295
     * PATH_MAX is typically 4096, so we can't reach overflow with real paths.
     * We'd need to mock with artificially large values or modify the function.
     */

    printf("SIZE_MAX = %zu\n", (size_t)SIZE_MAX);
    printf("PATH_MAX = %d\n", PATH_MAX);
    printf("Note: Integer overflow requires path lengths > %zu\n", SIZE_MAX / 2);
    printf("This is not achievable with PATH_MAX = %d\n", PATH_MAX);
    printf("⚠ Cannot realistically test integer overflow with system PATH_MAX limits\n");
    printf("\nConclusion: The theoretical integer overflow vulnerability exists\n");
    printf("but is not exploitable with real-world PATH_MAX constraints.\n");
}

/* ============ Main ============ */

int main(void) {
    printf("========================================\n");
    printf("normalize_path() Test Harness\n");
    printf("========================================\n");
    printf("Compiled with AddressSanitizer: ");
#ifdef __SANITIZE_ADDRESS__
    printf("YES\n");
#else
    printf("UNKNOWN (may still be enabled)\n");
#endif

    printf("\nRunning tests...\n");
    fflush(stdout);

    printf("Starting test 1...\n"); fflush(stdout);
    test_1_long_cwd_and_long_relative_path();
    printf("Starting test 2...\n"); fflush(stdout);
    test_2_many_dotdot_with_expansion();
    printf("Starting test 3...\n"); fflush(stdout);
    test_3_max_path_length();
    printf("Starting test 4...\n"); fflush(stdout);
    test_4_empty_and_special_segments();
    printf("Starting test 5...\n"); fflush(stdout);
    test_5_root_cwd_edge_case();
    printf("Starting test 6...\n"); fflush(stdout);
    test_6_trigger_line_575_check();
    printf("Starting test 7...\n"); fflush(stdout);
    test_7_artificial_overflow();

    printf("\n========================================\n");
    printf("All tests completed without crashes\n");
    printf("If AddressSanitizer detected issues, they would be reported above.\n");
    printf("========================================\n");

    return 0;
}
