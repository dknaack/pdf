#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define S(x) (str){(x), sizeof(x)-1}

typedef uintptr_t usize;
typedef uint64_t  u64;
typedef uint32_t  u32, b32;
typedef uint16_t  u16;
typedef uint8_t   u8;

typedef intptr_t isize;
typedef int64_t  i64;
typedef int32_t  i32;
typedef int16_t  i16;
typedef int8_t   i8;

typedef double f64;
typedef float  f32;

typedef struct {
	char *at;
	isize length;
} str;

typedef struct {
	str *at;
	isize count;
} str_array;

static b32
is_space(char c)
{
	b32 result = (c == ' ' || c == '\n' || c == '\t' || c == '\r' || c == '\f' || c == '\v');
	return result;
}

static str
trim(str s)
{
	while (s.length > 0 && is_space(*s.at)) {
		s.at++;
		s.length--;
	}

	while (s.length > 0 && is_space(s.at[s.length - 1])) {
		s.length--;
	}

	return s;
}

static str
substr(str s, isize start, isize end)
{
	str result = {0};
	result.at = s.at + start;
	result.length = end - start;
	return result;
}

static str_array
split(str s)
{
	str_array result = {0};
	isize end = 0;
	isize i = 0;

	s = trim(s);
	while (end < s.length) {
		while (end < s.length && !is_space(s.at[end])) {
			end++;
		}

		result.count++;
		while (end < s.length && is_space(s.at[end])) {
			end++;
		}
	}

	result.at = calloc(result.count, sizeof(*result.at));

	end = 0;
	while (end < s.length) {
		isize start = end;
		while (end < s.length && !is_space(s.at[end])) {
			end++;
		}

		result.at[i++] = substr(s, start, end);
		while (end < s.length && is_space(s.at[end])) {
			end++;
		}
	}

	return result;
}

int
main(void)
{
	str input = S(
		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Praesent "
		"libero tellus, rutrum vitae neque nec, volutpat pretium justo. Maecenas "
		"id enim a dui blandit tincidunt non at augue. Fusce augue ipsum, "
		"convallis ac massa vitae, malesuada interdum nisi. Fusce massa odio, "
		"iaculis at semper eget, sodales a metus. Ut mattis ante ut nisl"
		"pellentesque luctus. Interdum et malesuada fames ac ante ipsum primis "
		"in faucibus. Sed at tempus tortor. Donec ante lacus, porttitor non "
		"neque sed, venenatis dignissim ipsum. Aliquam cursus, risus ut "
		"vulputate venenatis, risus ipsum mollis ex, id scelerisque tellus ante "
		"et ipsum. Donec non urna eu tellus.");
	str_array words = split(input);

	int line_width = 32;
	int *cost = calloc(words.count + 1, sizeof(*cost));
	int *prev = calloc(words.count + 1, sizeof(*cost));
	for (isize j = 1; j <= words.count; j++) {
		cost[j] = INT32_MAX;

		int total_width = 0;
		for (isize i = j - 1; i >= 0; i--) {
			total_width += (i != 0) + words.at[i].length;

			int badness = line_width - total_width;
			if (badness < 0) {
				break;
			}

			int current_cost = (badness * badness) + cost[i];
			if (cost[j] > current_cost) {
				cost[j] = current_cost;
				prev[j] = i;
			}
		}
	}

	int *should_break = calloc(words.count + 1, sizeof(*should_break));
	for (int i = words.count; i > 0; i = prev[i]) {
		should_break[i] = true;
	}

	for (int i = 0; i < words.count; i++) {
		if (should_break[i]) {
			printf("\n");
		} else if (i != 0) {
			printf(" ");
		}

		printf("%.*s", (int)words.at[i].length, words.at[i].at);
	}

	printf("\n");
	return 0;
}
