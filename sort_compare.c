/*
 * 과제1. Compare sorting
 * 삽입 정렬(Insertion) / 병합 정렬(Merge) / 힙 정렬(Heap) 비교
 *
 * 컴파일:  gcc -O2 -Wall -o sort_compare sort_compare.c
 * 실행:    ./sort_compare            (Windows: sort_compare.exe)
 * 결과:    화면 출력 + results.csv + stability.csv 저장 (보고서 표/그래프용)
 *
 * 측정 항목
 *   - 시간(ms)         : 복사 시간은 빼고, 정렬 함수만 잰다
 *   - 비교 횟수        : key끼리 비교한 횟수
 *   - 이동 횟수        : 정렬 과정에서 발생한 이동/복사 횟수
 *   - 추가 메모리(B)   : 입력 배열 밖에 따로 잡은 메모리
 *   - 재귀 깊이        : 실제로 들어간 최대 재귀 깊이 (반복문만 쓰면 1)
 *   - 안정성           : 같은 key끼리 입력 순서(tag)가 유지되는지 실측 (맨 아래 별도 표)
 *
 * 입력 종류: random / sorted / reversed / nearly(거의 정렬됨) / duplicates(중복 많음)
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

/* 원소: key로 정렬하고, tag에는 "입력에서의 원래 순서"를 새겨 둔다 (안정성 측정용) */
typedef struct {
    int key;
    int tag;
} Record;

/* ---------- 설정 ---------- */
static const int SIZES[] = {1000, 10000, 50000, 100000, 200000, 1000000};
#define NUM_SIZES (int)(sizeof(SIZES) / sizeof(SIZES[0]))
#define INSERTION_LIMIT 200000 /* 삽입 정렬은 O(n^2)라 이 크기 초과 시 생략 */
#define SMALL_N 10000          /* 이 크기 이하는 여러 번 반복해 평균 */
#define SMALL_REPEAT 5
#define DUP_KEYS 100           /* duplicates 입력: key 종류가 100가지뿐 */
#define STABILITY_N 10000      /* 안정성 실험 크기 */

/* ---------- 측정용 전역 카운터 (정렬 함수가 직접 센다) ---------- */
static ll cmp_cnt;        /* 비교 횟수 */
static ll mv_cnt;         /* 이동 횟수 */
static size_t extra_bytes; /* 추가 메모리 */
static int depth_cur, depth_max;

static void reset_stats(void) {
    cmp_cnt = mv_cnt = 0;
    extra_bytes = 0;
    depth_cur = depth_max = 0;
}

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

/* ============================================================
 * 1. 삽입 정렬: 평균/최악 O(n^2), 최선 O(n), 안정, 추가 메모리 O(1)
 *    이동 = 한 칸씩 미는 횟수 + (움직인 원소마다 tmp 보관/복원 2회)
 * ============================================================ */
static void insertion_sort(Record *a, int n) {
    extra_bytes = sizeof(Record); /* tmp 한 칸 */
    depth_max = 1;
    for (int i = 1; i < n; i++) {
        Record t = a[i];
        int j = i - 1;
        int moved = 0;
        while (j >= 0) {
            cmp_cnt++;
            if (a[j].key > t.key) { /* '>=' 이면 불안정해진다 */
                a[j + 1] = a[j];
                mv_cnt++;
                moved = 1;
                j--;
            } else {
                break;
            }
        }
        if (moved) {
            a[j + 1] = t;
            mv_cnt += 2; /* tmp에 꺼내기 + 제자리에 넣기 */
        }
    }
}

/* ============================================================
 * 2. 병합 정렬: 항상 O(n log n), 안정, 추가 메모리 O(n)
 * ============================================================ */
static void merge_rec(Record *a, Record *tmp, int l, int r) {
    if (l >= r) return;
    depth_cur++;
    if (depth_cur > depth_max) depth_max = depth_cur;

    int m = l + (r - l) / 2;
    merge_rec(a, tmp, l, m);
    merge_rec(a, tmp, m + 1, r);

    int i = l, j = m + 1, k = l;
    while (i <= m && j <= r) {
        cmp_cnt++;
        if (a[i].key <= a[j].key) tmp[k++] = a[i++]; /* '<=' 라서 안정 */
        else                      tmp[k++] = a[j++];
        mv_cnt++;
    }
    while (i <= m) { tmp[k++] = a[i++]; mv_cnt++; }
    while (j <= r) { tmp[k++] = a[j++]; mv_cnt++; }
    memcpy(a + l, tmp + l, (size_t)(r - l + 1) * sizeof(Record));
    mv_cnt += r - l + 1;

    depth_cur--;
}

static void merge_sort(Record *a, int n) {
    Record *tmp = (Record *)malloc((size_t)n * sizeof(Record));
    if (!tmp) { fprintf(stderr, "malloc 실패\n"); exit(1); }
    extra_bytes = (size_t)n * sizeof(Record); /* 보조 배열 */
    merge_rec(a, tmp, 0, n - 1);
    free(tmp);
}

/* ============================================================
 * 3. 힙 정렬: 항상 O(n log n), 불안정, 추가 메모리 O(1)
 * ============================================================ */
static void swap_rec(Record *x, Record *y) {
    Record t = *x; *x = *y; *y = t;
    mv_cnt += 3; /* 교환 = 이동 3회 */
}

static void sift_down(Record *a, int i, int n) {
    while (1) {
        int l = 2 * i + 1, r = l + 1, big = l;
        if (l >= n) break;
        if (r < n) {
            cmp_cnt++;
            if (a[r].key > a[l].key) big = r;
        }
        cmp_cnt++;
        if (a[big].key > a[i].key) {
            swap_rec(&a[big], &a[i]);
            i = big;
        } else {
            break;
        }
    }
}

static void heap_sort(Record *a, int n) {
    extra_bytes = sizeof(Record); /* 교환용 tmp 한 칸 */
    depth_max = 1;                /* 반복문만 사용 */
    for (int i = n / 2 - 1; i >= 0; i--) sift_down(a, i, n); /* 힙 만들기 */
    for (int end = n - 1; end > 0; end--) {
        swap_rec(&a[0], &a[end]);
        sift_down(a, 0, end);
    }
}

/* ---------- 데이터 생성 ---------- */
typedef enum { RANDOM, SORTED, REVERSED, NEARLY, DUPLICATES, NUM_DIST } Dist;
static const char *DIST_NAME[] = {"random", "sorted", "reversed", "nearly", "duplicates"};

static int cmp_rec(const void *x, const void *y) {
    int a = ((const Record *)x)->key, b = ((const Record *)y)->key;
    return (a > b) - (a < b);
}

static void generate(Record *a, int n, Dist d) {
    unsigned int range = (d == DUPLICATES) ? DUP_KEYS : 1000000u;
    for (int i = 0; i < n; i++) a[i].key = (int)(rnd() % range);

    if (d == SORTED || d == NEARLY) qsort(a, (size_t)n, sizeof(Record), cmp_rec);
    if (d == REVERSED) {
        qsort(a, (size_t)n, sizeof(Record), cmp_rec);
        for (int i = 0; i < n / 2; i++) {
            Record t = a[i]; a[i] = a[n - 1 - i]; a[n - 1 - i] = t;
        }
    }
    if (d == NEARLY) { /* 정렬된 상태에서 약 1%만 무작위 위치 교환 */
        int swaps = n / 100 + 1;
        for (int s = 0; s < swaps; s++) {
            int i = (int)(rnd() % (unsigned)n), j = (int)(rnd() % (unsigned)n);
            Record t = a[i]; a[i] = a[j]; a[j] = t;
        }
    }
    for (int i = 0; i < n; i++) a[i].tag = i; /* 입력 순서를 새긴다 */
}

/* ---------- 검증 ---------- */
static int check_sorted(const Record *a, int n) {
    ll tag_sum = 0;
    for (int i = 0; i < n; i++) {
        if (i > 0 && a[i - 1].key > a[i].key) return 0;
        tag_sum += a[i].tag;
    }
    return tag_sum == (ll)n * (n - 1) / 2; /* 원소가 사라지거나 복제되지 않았는지 */
}

/* 같은 key끼리 tag가 오름차순이면 안정 */
static int check_stable(const Record *a, int n) {
    for (int i = 1; i < n; i++)
        if (a[i - 1].key == a[i].key && a[i - 1].tag > a[i].tag) return 0;
    return 1;
}

/* ---------- 알고리즘 테이블 ---------- */
typedef void (*SortFn)(Record *, int);
typedef struct {
    const char *name;
    SortFn fn;
    int limit;
    const char *theory_time;
    const char *theory_space;
    const char *theory_stable;
} Algo;

static const Algo ALGOS[] = {
    {"insertion", insertion_sort, INSERTION_LIMIT, "O(n^2)",     "O(1)", "yes"},
    {"merge",     merge_sort,     2000000000,      "O(n log n)", "O(n)", "yes"},
    {"heap",      heap_sort,      2000000000,      "O(n log n)", "O(1)", "no"},
};
#define NUM_ALGOS (int)(sizeof(ALGOS) / sizeof(ALGOS[0]))

int main(void) {
    FILE *csv = fopen("results.csv", "w");
    if (!csv) { fprintf(stderr, "results.csv 생성 실패\n"); return 1; }
    fprintf(csv, "distribution,n,algorithm,time_ms,comparisons,moves,extra_bytes,max_depth\n");

    printf("%-10s %8s %-10s %12s %15s %15s %12s %6s\n",
           "data", "n", "algorithm", "time(ms)", "comparisons", "moves", "extra(B)", "depth");
    printf("----------------------------------------------------------------------------------------\n");

    for (int d = 0; d < NUM_DIST; d++) {
        for (int s = 0; s < NUM_SIZES; s++) {
            int n = SIZES[s];
            Record *orig = (Record *)malloc((size_t)n * sizeof(Record));
            Record *work = (Record *)malloc((size_t)n * sizeof(Record));
            if (!orig || !work) { fprintf(stderr, "malloc 실패\n"); return 1; }

            rng_state = 2463534242u + (unsigned)(d * 100 + s); /* 케이스별 고정 시드 */
            generate(orig, n, (Dist)d);

            for (int k = 0; k < NUM_ALGOS; k++) {
                if (n > ALGOS[k].limit) {
                    printf("%-10s %8d %-10s %12s\n", DIST_NAME[d], n, ALGOS[k].name, "skipped");
                    fprintf(csv, "%s,%d,%s,,,,,\n", DIST_NAME[d], n, ALGOS[k].name);
                    continue;
                }
                int repeat = (n <= SMALL_N) ? SMALL_REPEAT : 1;
                double total = 0.0;
                ll cmps = 0, mvs = 0;
                size_t mem = 0;
                int depth = 0;
                for (int r = 0; r < repeat; r++) {
                    memcpy(work, orig, (size_t)n * sizeof(Record));
                    reset_stats();
                    double t0 = now_ms();
                    ALGOS[k].fn(work, n);
                    total += now_ms() - t0;
                    cmps = cmp_cnt; mvs = mv_cnt; mem = extra_bytes; depth = depth_max;
                    if (!check_sorted(work, n)) {
                        fprintf(stderr, "오류: %s 정렬 결과가 틀림 (n=%d)\n", ALGOS[k].name, n);
                        return 1;
                    }
                }
                double avg = total / repeat;
                printf("%-10s %8d %-10s %12.3f %15lld %15lld %12zu %6d\n",
                       DIST_NAME[d], n, ALGOS[k].name, avg, cmps, mvs, mem, depth);
                fprintf(csv, "%s,%d,%s,%.3f,%lld,%lld,%zu,%d\n",
                        DIST_NAME[d], n, ALGOS[k].name, avg, cmps, mvs, mem, depth);
                fflush(stdout);
            }
            free(orig);
            free(work);
        }
        printf("\n");
    }
    fclose(csv);

    /* ---------- 안정성 실험 ---------- */
    FILE *scsv = fopen("stability.csv", "w");
    if (!scsv) { fprintf(stderr, "stability.csv 생성 실패\n"); return 1; }
    fprintf(scsv, "algorithm,theory_time,theory_space,theory_stable,measured_stable\n");

    printf("=== 안정성 실측 (n=%d, key %d종류 -> 같은 key가 매우 많음) ===\n", STABILITY_N, DUP_KEYS);
    printf("%-10s %-12s %-8s %-14s %-14s\n", "algorithm", "time", "space", "stable(theory)", "stable(measured)");
    Record *base = (Record *)malloc(STABILITY_N * sizeof(Record));
    Record *work = (Record *)malloc(STABILITY_N * sizeof(Record));
    rng_state = 2463534242u + 999;
    generate(base, STABILITY_N, DUPLICATES);
    for (int k = 0; k < NUM_ALGOS; k++) {
        memcpy(work, base, STABILITY_N * sizeof(Record));
        reset_stats();
        ALGOS[k].fn(work, STABILITY_N);
        const char *res = check_stable(work, STABILITY_N) ? "yes" : "no";
        printf("%-10s %-12s %-8s %-14s %-14s\n",
               ALGOS[k].name, ALGOS[k].theory_time, ALGOS[k].theory_space, ALGOS[k].theory_stable, res);
        fprintf(scsv, "%s,%s,%s,%s,%s\n",
                ALGOS[k].name, ALGOS[k].theory_time, ALGOS[k].theory_space, ALGOS[k].theory_stable, res);
    }
    free(base);
    free(work);
    fclose(scsv);

    printf("\n결과가 results.csv, stability.csv 에 저장되었습니다.\n");
    return 0;
}
