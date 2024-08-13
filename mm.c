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
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
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

// block pointer인 bp가 주어졌을 때, 해당 block의 header와 footer의 주소를 계산하기
#define HDR_P(bp) ((char *)(bp) - W_SIZE)                       // bp-(1word)
#define FTR_P(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DW_SIZE) // bp+ (block size)

// block pointer인 bp가 주어졌을 때, 해당 block의 이전, 다음 block 주소를 계산하기
#define NEXT_BLK_P(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - W_SIZE)))  // bp+(현재 block의 size)
#define PREV_BLK_P(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DW_SIZE))) // bp-(이전 block의 size)

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{

    // 초기의 빈 heap 생성하기

    /*header의 구조 설정*/
    static char *heap_list_p;
    if ((heap_list_p = mem_sbrk(4 * W_SIZE)) == (void *)-1)
        return -1;
    PUT(heap_list_p, 0);
    PUT(heap_list_p + (2 * W_SIZE), 1);
    PUT(heap_list_p + (1 * W_SIZE), PACK(DW_SIZE, 1));
    PUT(heap_list_p + (2 * W_SIZE), PACK(DW_SIZE, 1));

    heap_list_p += (2 * W_SIZE); //*heap_list_p는 항상 프롤로그 블록을 가리킴

    /*빈 heap을 chunksize 만큼 확장 */
    if (extend_heap(CHUNK_SIZE / W_SIZE) == NULL) // chunk_size를 메모리에서 가져오는데 실패->heap확장불가
        return -1;
    return 0;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /*double word 정렬을 유지하기 위해 짝수 word를 할당하기*/
    size = (words % 2) ? (words + 1) * W_SIZE : words * W_SIZE; // 나머지 결과(0/1)에 따라 size 값 다르게 설정
    if ((long)(bp = mem_sbrk(size)) == -1)                      // memory sytem에서 size만큼 가져오는데 실패한경우
        return NULL;

    /*현재 bp가 가리키고 있는 가용 block의 header/footer 그리고 epilogue header를 초기화*/
    PUT(HDR_P(bp), PACK(size, 0));
    PUT(FTR_P(bp), PACK(size, 0));
    PUT(HDR_P(EXT_BLK_P(bp)), PACK(0, 1));

    /*만약 이전 블록이 가용상태*/
    return (coalesce(bp));
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
        return NULL;
    else
    {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}
