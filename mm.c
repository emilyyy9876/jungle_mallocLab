/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "team 4",
    /* First member's full name */
    "Emily Kang",
    /* First member's email address */
    "strongwomankky@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// 기본 상수들과 메크로들
#define W_SIZE 4             // word의 사이즈(header,footer 포함)(bytes)
#define DW_SIZE 8            // double word의 사이즈(bytes)
#define CHUNK_SIZE (1 << 12) // 초기 heap 확장 사이즈(bytes)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

// 비트 연산을 이용한 데이터의 저장과 추출

// size, allocated라는 두 인자를 받아들이고, 비트(OR('|'))연산으로 결합하여 하나의 값으로 만드는 것
#define PACK(size, alloc) ((size) | (alloc))

// 주소 p에 있는, 한 word를 읽거나. 한 word에 쓰기
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// 주소 p에서의 size와 allocated 필드를 읽기

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

// block pointer인 ptr가 주어졌을 때, 해당 block의 header와 footer의 주소를 계산하기
#define HDR_P(ptr) ((char *)(ptr) - W_SIZE)                         // ptr-(1word)
#define FTR_P(ptr) ((char *)(ptr) + GET_SIZE(HDR_P(ptr)) - DW_SIZE) // ptr+ (block size)

// block pointer인 ptr가 주어졌을 때, 해당 block의 이전, 다음 block 주소를 계산하기
#define NEXT_BLK_P(ptr) ((char *)(ptr) + GET_SIZE(((char *)(ptr) - W_SIZE)))  // ptr+(현재 block의 size)
#define PREV_BLK_P(ptr) ((char *)(ptr) - GET_SIZE(((char *)(ptr) - DW_SIZE))) // ptr-(이전 block의 size)

static char *heap_list_p;
static char *srch_ptr;
static void *extend_heap(size_t words);
static void *find_fit(size_t asize);
static void place(void *ptr, size_t asize);
static void *coalesce(void *ptr);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{

    // 초기의 빈 heap 생성하기

    /*header의 구조 설정*/
    if ((heap_list_p = mem_sbrk(4 * W_SIZE)) == (void *)-1)
        return -1;
    PUT(heap_list_p, 0);
    PUT(heap_list_p + (2 * W_SIZE), 1);
    PUT(heap_list_p + (1 * W_SIZE), PACK(DW_SIZE, 1));
    PUT(heap_list_p + (2 * W_SIZE), PACK(DW_SIZE, 1));

    heap_list_p += (2 * W_SIZE); //*heap_list_p는 항상 프롤로그 블록을 가리킴
    srch_ptr = heap_list_p;
    /*빈 heap을 chunksize 만큼 확장 */
    if (extend_heap(CHUNK_SIZE / W_SIZE) == NULL) // chunk_size를 메모리에서 가져오는데 실패->heap확장불가
        return -1;
    return 0;
}

static void *extend_heap(size_t words)
{
    char *ptr;
    size_t size;

    /*double word 정렬을 유지하기 위해 짝수 word를 할당하기*/
    size = (words % 2) ? (words + 1) * W_SIZE : words * W_SIZE; // 나머지 결과(0/1)에 따라 size 값 다르게 설정
    if ((long)(ptr = mem_sbrk(size)) == -1)                     // memory sytem에서 size만큼 가져오는데 실패한경우
        return NULL;

    /*현재 ptr가 가리키고 있는 가용 block의 header/footer 그리고 epilogue header를 초기화*/
    PUT(HDR_P(ptr), PACK(size, 0));
    PUT(FTR_P(ptr), PACK(size, 0));
    PUT(HDR_P(NEXT_BLK_P(ptr)), PACK(0, 1));

    /*힙 extend 후, 만약 이전 블록이 free 상태라면, 이전블록과 연결하기*/
    return (coalesce(ptr));
}

/* first_fit 구현 */
// static void *find_fit(size_t asize)
// {
//     void *ptr;
//     for (ptr = heap_list_p; GET_SIZE(HDR_P(ptr)); ptr = NEXT_BLK_P(ptr))
//     {
//         if (!GET_ALLOC(HDR_P(ptr)) && (asize <= GET_SIZE(HDR_P(ptr))))
//         {
//             return ptr;
//         }
//     }
//     return NULL;
// }

/* next_fit 구현*/
static void *find_fit(size_t asize)
{
    void *ptr;
    // 이전에 search가 끝났던 부분부터 찾기
    for (ptr = srch_ptr; GET_SIZE(HDR_P(ptr)) > 0; ptr = NEXT_BLK_P(ptr))
    {
        if (!GET_ALLOC(HDR_P(ptr)) && (asize <= GET_SIZE(HDR_P(ptr))))
        {
            srch_ptr = ptr;
            return ptr;
        }
    }
    // 만약,fit한 block을 못 찾았을 경우---> heap_list_p부터,
    for (ptr = heap_list_p; ptr != srch_ptr; ptr = NEXT_BLK_P(ptr))
    {
        if (!GET_ALLOC(HDR_P(ptr)) && (asize <= GET_SIZE(HDR_P(ptr))))
        {
            srch_ptr = ptr;
            return ptr;
        }
    }
    return NULL;
}

/*place 구현*/
static void place(void *ptr, size_t asize)
{
    size_t csize = GET_SIZE(HDR_P(ptr));

    if ((csize - asize) >= (2 * DW_SIZE))
    {
        PUT(HDR_P(ptr), PACK(asize, 1));
        PUT(FTR_P(ptr), PACK(asize, 1));

        ptr = NEXT_BLK_P(ptr);

        PUT(HDR_P(ptr), PACK(csize - asize, 0));
        PUT(FTR_P(ptr), PACK(csize - asize, 0));
    }
    else
    {
        PUT(HDR_P(ptr), PACK(csize, 1));
        PUT(FTR_P(ptr), PACK(csize, 1));
    }
}
/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
    //     return NULL;
    // else
    // {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }

    size_t asize;      /*조정된 블록의 사이즈인, 변수 asize */
    size_t extendsize; /*만약 할당할 만큼의 충분히 큰 블록을 찾지 못했을 때, heap 추가 확장할 크기*/
    char *ptr;

    if (size == 0)
        return NULL;

    /*size 크기에 따라, asize 값(할당 요청 크기, header&footer 크기도 포함되어야 함) 설정*/
    if (size <= DW_SIZE)
        asize = 2 * DW_SIZE;
    else
        asize = ALIGN(size + ALIGNMENT);

    /*free list를 검색해서, asize 크기에 맞는 가용블록 찾기*/
    if ((ptr = find_fit(asize)) != NULL)
    {
        place(ptr, asize);
        return ptr;
    }
    /*탐색 후, 맞는 블록 없을 때, heap 확장하기*/
    extendsize = MAX(asize, CHUNK_SIZE); /*asize, chunk_size 중 큰 값을 추가 확장값으로 결정*/
    if ((ptr = extend_heap(extendsize / W_SIZE)) == NULL)
        return NULL;
    place(ptr, asize);
    return ptr;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDR_P(ptr));

    // 현재 block를 free 상태로 설정
    PUT(HDR_P(ptr), PACK(size, 0));
    PUT(FTR_P(ptr), PACK(size, 0));
    coalesce(ptr);
}

static void *coalesce(void *ptr)
{
    /*prev, next block의 allocation 상태를 나타내는 변수(0/1) */
    size_t prev_alloc = GET_ALLOC(FTR_P(PREV_BLK_P(ptr)));
    size_t next_alloc = GET_ALLOC(HDR_P(NEXT_BLK_P(ptr)));

    size_t size = GET_SIZE(HDR_P(ptr));

    /*case1 _ prev & next block이 모두 allocated 상태*/
    if (prev_alloc && next_alloc)
    {
        return ptr;
    }

    /*case2 _ next block만 free 상태*/
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDR_P(NEXT_BLK_P(ptr))); // 새로운 사이즈
        PUT(HDR_P(ptr), PACK(size, 0));           // 현재 블록의 header, footer 갱신
        PUT(FTR_P(ptr), PACK(size, 0));
    }
    /*case3 _ prev block만 free 상태*/
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDR_P(PREV_BLK_P(ptr)));
        PUT(HDR_P(PREV_BLK_P(ptr)), PACK(size, 0));
        PUT(FTR_P(ptr), PACK(size, 0));

        ptr = PREV_BLK_P(ptr); // ptr주소를 prev block의 ptr주소로 설정
    }
    /*case4 _ prev & next 모두 free 상태*/
    else
    {
        size = size + GET_SIZE(FTR_P(NEXT_BLK_P(ptr))) + GET_SIZE(HDR_P(PREV_BLK_P(ptr)));
        PUT(HDR_P(PREV_BLK_P(ptr)), PACK(size, 0));
        PUT(FTR_P(NEXT_BLK_P(ptr)), PACK(size, 0));

        ptr = PREV_BLK_P(ptr); // ptr주소를 prev block의 ptr주소로 설정
    }

    // 빈 가용리스트끼리 연결 후, srch_ptr를 갱신해야 하는 경우 처리
    if (HDR_P(ptr) <= srch_ptr && srch_ptr <= FTR_P(ptr))
    {
        srch_ptr = ptr;
    }
    return ptr;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)
    {
        return mm_malloc(size);
    }
    if (size <= 0)
    {
        mm_free(ptr);
        return NULL;
    }
    // ptr
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = GET_SIZE(HDR_P(oldptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
