/* see LICENSE for licensing details */

static void *
mem_clear(u8 *p, u8 c, size len)
{
	while (len) p[--len] = c;
	return p;
}

#define alloc(a, t, n) alloc_(a, sizeof(t), _Alignof(t), n)
__attribute((malloc, alloc_size(4, 2), alloc_align(3)))
static void *
alloc_(Arena *a, size len, size align, size count)
{
	size padding   = -(uintptr_t)a->beg & (align - 1);
	size available = a->end - a->beg - padding;
	if (available < 0 || count > available / len) {
		/* TODO: handle error */
		u8 *t = 0; *t = 1;
	}
	void *p = a->beg + padding;
	a->beg += count * len;
	return mem_clear(p, 0, count * len);
}
