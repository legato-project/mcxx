#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "dhash_ptr.h"
#include "mem.h"

typedef
struct bucket_ptr_tag
{
    const char* key;
    dhash_ptr_info_t info;
    struct bucket_ptr_tag* next;
} bucket_ptr_t;

enum { MAX_BUCKET_SIZE = 76003 };
const static int prime_set[] = { 5, 11, 23, 53, 131, 317, 787, 1951, 4877, 12163, 30403, MAX_BUCKET_SIZE };
const static int last_prime = (sizeof(prime_set) / sizeof(prime_set[0])) - 1;

#ifdef __GNUC__
  #if __GNUC__ == 4
     #if __GNUC_MINOR__ >= 6
       #define STATIC_INLINE static inline
     #else
       // This is a workaround for GCC 4.4 generating wrong code
       #define STATIC_INLINE static
     #endif
  #else
     #define STATIC_INLINE static inline
  #endif
#else
   #define STATIC_INLINE static inline
#endif

STATIC_INLINE uint32_t Murmur3_32(const char* ptr);

struct dhash_ptr_tag
{
    bucket_ptr_t **buckets;
    int num_buckets_idx;
    int num_items;
};

dhash_ptr_t* dhash_ptr_new(int initial_size)
{
    if (initial_size < 0) abort();

    dhash_ptr_t* result = xmalloc(sizeof(*result));

    result->num_items = 0;

    if (initial_size >= MAX_BUCKET_SIZE)
    {
        result->num_buckets_idx = last_prime;
    }
    else
    {
        // Round to next prime
        result->num_buckets_idx = 0;
        while (prime_set[result->num_buckets_idx] < initial_size)
        {
            result->num_buckets_idx++;
        }
    }

    result->buckets = xcalloc(sizeof(*(result->buckets)), prime_set[result->num_buckets_idx]);

    return result;
}

static void free_bucket_list(bucket_ptr_t* bucket)
{
    if (bucket == NULL)
        return;

    free_bucket_list(bucket->next);
    xfree(bucket);
}

void dhash_ptr_destroy(dhash_ptr_t* dhash)
{
    int num_buckets = prime_set[dhash->num_buckets_idx];

    int i;
    for (i = 0; i < num_buckets; i++)
    {
        free_bucket_list(dhash->buckets[i]);
    }

    xfree(dhash->buckets);
}

void* dhash_ptr_query(dhash_ptr_t* dhash, const char* key)
{
    if (key == NULL) abort();

    int num_buckets = prime_set[dhash->num_buckets_idx];
    uint32_t hash = Murmur3_32(key) % num_buckets;

    bucket_ptr_t* b = dhash->buckets[hash];
    while (b != NULL)
    {
        if (b->key == key)
        {
            return b->info;
        }
        b = b->next;
    }

    return NULL;
}


static void dhash_ptr_do_insert(dhash_ptr_t* dhash, const char* key, dhash_ptr_info_t info)
{
    int num_buckets = prime_set[dhash->num_buckets_idx];
    uint32_t hash = Murmur3_32(key) % num_buckets;

    bucket_ptr_t* b = dhash->buckets[hash];

    while (b != NULL)
    {
        if (b->key == key)
        {
            // Update
            b->info = info;
            return;
        }
        b = b->next;
    }

    // Insert
    b = xmalloc(sizeof(*b));
    b->key = key;
    b->info = info;
    b->next = dhash->buckets[hash];
    dhash->buckets[hash] = b;

    dhash->num_items++;
}

static void dhash_ptr_increase_rehash(dhash_ptr_t* dhash)
{
    // Do not rehash anymore
    if (dhash->num_buckets_idx == last_prime)
        return;

    int num_old_buckets = prime_set[dhash->num_buckets_idx];
    bucket_ptr_t **old_buckets = dhash->buckets;

    dhash->num_buckets_idx++;
    int num_new_buckets = prime_set[dhash->num_buckets_idx];
    bucket_ptr_t **new_buckets = xcalloc(sizeof(*new_buckets), num_new_buckets);

    int i;
    for (i = 0; i < num_old_buckets; i++)
    {
        bucket_ptr_t* old_b = old_buckets[i];

        while (old_b != NULL)
        {
            uint32_t new_hash = Murmur3_32(old_b->key) % num_new_buckets;

            bucket_ptr_t* new_b = xmalloc(sizeof(*new_b));
            new_b->key = old_b->key;
            new_b->info = old_b->info;
            new_b->next = new_buckets[new_hash];
            new_buckets[new_hash] = new_b;

            old_b = old_b->next;
        }
    }

    for (i = 0; i < num_old_buckets; i++)
    {
        free_bucket_list(old_buckets[i]);
    }
    xfree(old_buckets);

    dhash->buckets = new_buckets;
}

void dhash_ptr_insert(dhash_ptr_t* dhash, const char* key, dhash_ptr_info_t info)
{
    if (key == NULL) abort();
    if (info == NULL) abort();

    int num_buckets = prime_set[dhash->num_buckets_idx];
    if (((num_buckets * 3) / 4) < dhash->num_items)
    {
        dhash_ptr_increase_rehash(dhash);
    }

    dhash_ptr_do_insert(dhash, key, info);
}

void dhash_ptr_remove(dhash_ptr_t* dhash, const char* key)
{
    if (key == NULL) abort();

    int num_buckets = prime_set[dhash->num_buckets_idx];
    uint32_t hash = Murmur3_32(key) % num_buckets;

    bucket_ptr_t** b = &(dhash->buckets[hash]);

    while ((*b) != NULL)
    {
        if ((*b)->key == key)
        {
            bucket_ptr_t* current = *b;
            *b = (*b)->next;
            xfree(current);
            dhash->num_items--;
            return;
        }
        b = &((*b)->next);
    }

    // Not found
}

void dhash_ptr_walk(dhash_ptr_t* dhash, dhash_ptr_walk_fn walk_fn, void *walk_info)
{
    int num_buckets = prime_set[dhash->num_buckets_idx];

    int i;
    for (i = 0; i < num_buckets; i++)
    {
        bucket_ptr_t* b = dhash->buckets[i];
        while (b != NULL)
        {
            walk_fn(b->key, b->info, walk_info);
            b = b->next;
        }
    }
}

// Hash function
// Taken from wikipedia
STATIC_INLINE uint32_t Murmur3_32(const char* ptr)
{
	static const uint32_t c1 = 0xcc9e2d51;
	static const uint32_t c2 = 0x1b873593;
	static const uint32_t r1 = 15;
	static const uint32_t r2 = 13;
	static const uint32_t m = 5;
	static const uint32_t n = 0xe6546b64;

    // Size of a pointer
    uint32_t len = sizeof(ptr);
    const char* key = (const char*)&ptr;

	uint32_t hash = 0;
 
	uint32_t* keydata = (uint32_t*) key; //used to extract 32 bits at a time
	int keydata_it = 0;
 
	while (len >= 4)
	{
		uint32_t k = keydata[keydata_it++];
		len -= 4;
 
		k *= c1;
		k = (k << r1) | (k >> (32-r1));
		k *= c2;
 
		hash ^= k;
		hash = ((hash << r2) | (hash >> (32-r2)) * m) + n;
	}
 
	const uint8_t * tail = (const uint8_t*)(keydata + keydata_it*4);
	uint32_t k1 = 0;
 
	switch(len & 3) {
	case 3:
		k1 ^= tail[2] << 16;
	case 2:
		k1 ^= tail[1] << 8;
	case 1:
		k1 ^= tail[0];
 
		k1 *= c1;
		k1 = (k1 << r1) | (k1 >> (32-r1));
		k1 *= c2;
		hash ^= k1;
	}
 
	hash ^= len;
	hash ^= (hash >> 16);
	hash *= 0x85ebca6b;
	hash ^= (hash >> 13);
	hash *= 0xc2b2ae35;
	hash ^= (hash >> 16);
 
	return hash;
}