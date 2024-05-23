/* see LICENSE for licensing details */
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct timespec os_filetime;

typedef struct {
	size        filesize;
	os_filetime timestamp;
} os_file_stats;

static os_file_stats
os_get_file_stats(char *file)
{
	struct stat sb;
	if (stat(file, &sb) < 0)
		return (os_file_stats){0};
	return (os_file_stats){
		.filesize  = sb.st_size,
		.timestamp = sb.st_mtim,
	};
}

static s8
os_read_file(char *file)
{
	s8 ret = {0};
	i32 fd = open(file, O_RDONLY);
	if (fd < 0)
		return ret;
	ret.len  = lseek(fd, 0L, SEEK_END);
	ret.data = malloc(ret.len);
	lseek(fd, 0L, SEEK_SET);
	if (ret.data == NULL) {
		ret.len = 0;
		close(fd);
		return ret;
	}
	if (ret.len != read(fd, ret.data, ret.len)) {
		ret.len = 0;
		free(ret.data);
	}
	close(fd);
	return ret;
}

static i32
os_compare_filetime(os_filetime a, os_filetime b)
{
	return (a.tv_sec - b.tv_sec) + (a.tv_nsec - b.tv_nsec);
}
