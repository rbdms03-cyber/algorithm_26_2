/*
 * 과제1. Compare sorting
 * 삽입 정렬(Insertion) / 병합 정렬(Merge) / 힙 정렬(Heap) 성능 비교
 *
 * 컴파일:  gcc -O2 -Wall -o sort_compare sort_compare.c
 * 실행:    ./sort_compare            (Windows: sort_compare.exe)
 * 결과:    화면 출력 + results.csv 저장 (보고서 표/그래프용)
 *
 * 측정 항목: 실행 시간(ms), 비교 횟수
 * 데이터 종류: random / sorted / reversed / nearly(거의 정렬됨)
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#endif

typedef long long ll;

/* ---------- 설정 ---------- */
static const int SIZES[] = {1000, 10000, 50000, 100000, 200000, 1000000};
#define NUM_SIZES (int)(sizeof(SIZES) / sizeof(SIZES[0]))
#define INSERTION_LIMIT 200000 /* 삽입 정렬은 O(n^2)라 이 크기 초과 시 생략 */
#define SMALL_N 10000          /* 이 크기 이하는 여러 번 반복해 평균 */
#define SMALL_REPEAT 5

static ll cmp_cnt; /* 비교 횟수 카운터 */

/* ---------- 타이머 ---------- */
static double now_ms(void) {
#ifdef _WIN32
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
#endif
}

/* ---------- 난수 (재현 가능하도록 고정 시드 xorshift) ---------- */
static unsigned int rng_state = 2463534242u;
static unsigned int rnd(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

/* ---------- 1. 삽입 정렬: 평균/최악 O(n^2), 최선 O(n) ---------- */
static void insertion_sort(int *a, int n) {
    for (int i = 1; i < n; i++) {
        int key = a[i];
        int j = i - 1;
        while (j >= 0) {
            cmp_cnt++;
            if (a[j] > key) {
                a[j + 1] = a[j];
                j--;
            } else {
                break;
            }
        }
        a[j + 1] = key;
    }
}

/* ---------- 2. 병합 정렬: 항상 O(n log n), 추가 메모리 O(n) ---------- */
static void merge_rec(int *a, int *tmp, int l, int r) {
    if (l >= r) return;
    int m = l + (r - l) / 2;
    merge_rec(a, tmp, l, m);
    merge_rec(a, tmp, m + 1, r);

    int i = l, j = m + 1, k = l;
    while (i <= m && j <= r) {
        cmp_cnt++;
        if (a[i] <= a[j]) tmp[k++] = a[i++];
        else              tmp[k++] = a[j++];
    }
    while (i <= m) tmp[k++] = a[i++];
    while (j <= r) tmp[k++] = a[j++];
    memcpy(a + l, tmp + l, (size_t)(r - l + 1) * sizeof(int));
}

static void merge_sort(int *a, int n) {
    int *tmp = (int *)malloc((size_t)n * sizeof(int));
    if (!tmp) { fprintf(stderr, "malloc 실패\n"); exit(1); }
    merge_rec(a, tmp, 0, n - 1);
    free(tmp);
}

/* ---------- 3. 힙 정렬: 항상 O(n log n), 제자리(in-place) ---------- */
static void sift_down(int *a, int i, int n) {
    while (1) {
        int l = 2 * i + 1, r = l + 1, big = l;
        if (l >= n) break;
        if (r < n) {
            cmp_cnt++;
            if (a[r] > a[l]) big = r;
        }
        cmp_cnt++;
        if (a[big] > a[i]) {
            int t = a[big]; a[big] = a[i]; a[i] = t;
            i = big;
        } else {
            break;
        }
    }
}

static void heap_sort(int *a, int n) {
    for (int i = n / 2 - 1; i >= 0; i--) sift_down(a, i, n); /* 힙 만들기 */
    for (int end = n - 1; end > 0; end--) {
        int t = a[0]; a[0] = a[end]; a[end] = t;
        sift_down(a, 0, end);
    }
}

/* ---------- 데이터 생성 ---------- */
typedef enum { RANDOM, SORTED, REVERSED, NEARLY, NUM_DIST } Dist;
static const char *DIST_NAME[] = {"random", "sorted", "reversed", "nearly"};

static int cmp_int(const void *x, const void *y) {
    int a = *(const int *)x, b = *(const int *)y;
    return (a > b) - (a < b);
}

static void generate(int *a, int n, Dist d) {
    for (int i = 0; i < n; i++) a[i] = (int)(rnd() % 1000000);
    if (d == SORTED || d == NEARLY) qsort(a, (size_t)n, sizeof(int), cmp_int);
    if (d == REVERSED) {
        qsort(a, (size_t)n, sizeof(int), cmp_int);
        for (int i = 0; i < n / 2; i++) {
            int t = a[i]; a[i] = a[n - 1 - i]; a[n - 1 - i] = t;
        }
    }
    if (d == NEARLY) { /* 정렬된 상태에서 약 1%만 무작위 위치 교환 */
        int swaps = n / 100 + 1;
        for (int s = 0; s < swaps; s++) {
            int i = (int)(rnd() % (unsigned)n), j = (int)(rnd() % (unsigned)n);
            int t = a[i]; a[i] = a[j]; a[j] = t;
        }
    }
}

static int is_sorted(const int *a, int n) {
    for (int i = 1; i < n; i++)
        if (a[i - 1] > a[i]) return 0;
    return 1;
}

/* ---------- 알고리즘 테이블 ---------- */
typedef void (*SortFn)(int *, int);
typedef struct { const char *name; SortFn fn; int limit; } Algo;

static const Algo ALGOS[] = {
    {"insertion", insertion_sort, INSERTION_LIMIT},
    {"merge",     merge_sort,     2000000000},
    {"heap",      heap_sort,      2000000000},
};
#define NUM_ALGOS (int)(sizeof(ALGOS) / sizeof(ALGOS[0]))

int main(void) {
    FILE *csv = fopen("results.csv", "w");
    if (!csv) { fprintf(stderr, "results.csv 생성 실패\n"); return 1; }
    fprintf(csv, "distribution,n,algorithm,time_ms,comparisons\n");

    printf("%-9s %9s %-10s %14s %16s\n", "data", "n", "algorithm", "time(ms)", "comparisons");
    printf("------------------------------------------------------------\n");

    for (int d = 0; d < NUM_DIST; d++) {
        for (int s = 0; s < NUM_SIZES; s++) {
            int n = SIZES[s];
            int *orig = (int *)malloc((size_t)n * sizeof(int));
            int *work = (int *)malloc((size_t)n * sizeof(int));
            if (!orig || !work) { fprintf(stderr, "malloc 실패\n"); return 1; }

            rng_state = 2463534242u + (unsigned)(d * 100 + s); /* 케이스별 고정 시드 */
            generate(orig, n, (Dist)d);

            for (int k = 0; k < NUM_ALGOS; k++) {
                if (n > ALGOS[k].limit) {
                    printf("%-9s %9d %-10s %14s %16s\n", DIST_NAME[d], n, ALGOS[k].name, "skipped", "-");
                    fprintf(csv, "%s,%d,%s,,\n", DIST_NAME[d], n, ALGOS[k].name);
                    continue;
                }
                int repeat = (n <= SMALL_N) ? SMALL_REPEAT : 1;
                double total = 0.0;
                ll cmps = 0;
                for (int r = 0; r < repeat; r++) {
                    memcpy(work, orig, (size_t)n * sizeof(int));
                    cmp_cnt = 0;
                    double t0 = now_ms();
                    ALGOS[k].fn(work, n);
                    total += now_ms() - t0;
                    cmps = cmp_cnt;
                    if (!is_sorted(work, n)) {
                        fprintf(stderr, "오류: %s 정렬 결과가 틀림 (n=%d)\n", ALGOS[k].name, n);
                        return 1;
                    }
                }
                double avg = total / repeat;
                printf("%-9s %9d %-10s %14.3f %16lld\n", DIST_NAME[d], n, ALGOS[k].name, avg, cmps);
                fprintf(csv, "%s,%d,%s,%.3f,%lld\n", DIST_NAME[d], n, ALGOS[k].name, avg, cmps);
                fflush(stdout);
            }
            free(orig);
            free(work);
        }
        printf("\n");
    }
    fclose(csv);
    printf("결과가 results.csv 에 저장되었습니다.\n");
    return 0;
}
