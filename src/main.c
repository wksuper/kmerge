#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

static const char *g_magic = "KmergeV1";

static long filesize(FILE *f)
{
	long cur, size;

	cur = ftell(f);
	fseek(f, 0L, SEEK_END);
	size = ftell(f);
	fseek(f, cur, SEEK_SET);
	return size;
}

static int kmerge(const char *outfile, char *infiles[], uint32_t infile_num)
{
	FILE *fout = NULL;
        long header_pos = 0, data_pos = 0;
	size_t i = 0;
	int ret = 0;

	fout = fopen(outfile, "wb");
	if (!fout) {
		printf("Unable to open %s\n", outfile);
                ret = -1;
		goto error;
	}

	fwrite(g_magic, strlen(g_magic), 1, fout);
        fwrite(&infile_num, sizeof(infile_num), 1, fout);

        header_pos = ftell(fout);
        data_pos = header_pos + infile_num * sizeof(uint32_t);

	for (i = 0; i < infile_num; ++i) {
                FILE *fin = NULL;
                uint32_t size = 0;
                void *buf = NULL;

		fin = fopen(infiles[i], "rb");
		if (!fin) {
                        printf("Unable to open %s\n", infiles[i]);
                        ret = -1;
			goto cleanup;
		}

                size = filesize(fin);

                /* write fin's size into header of fout */
                fseek(fout, header_pos, SEEK_SET);
                fwrite(&size, sizeof(size), 1, fout);
                header_pos = ftell(fout);

                if (size > 0) {
                        buf = malloc(size);
                        if (!buf) {
                                printf("Failed to alloc %u bytes\n", size);
                                ret = -1;
                                goto cleanup;
                        }

                        if (fread(buf, size, 1, fin) != 1) {
                                printf("Failed to read %s\n", infiles[i]);
                                ret = -1;
                                goto cleanup;
                        }
                        /* write fin's data into fout*/
                        fseek(fout, data_pos, SEEK_SET);
                        fwrite(buf, size, 1, fout);
                        data_pos = ftell(fout);
                }

cleanup:
                if (buf)
                        free(buf);
                if (fin)
                        fclose(fin);
                if (ret < 0)
                        break;
	}

error:
	if (fout) {
		fclose(fout);
	}
	return ret;
}

static int ksplit(const char *mergedfile)
{
        FILE *fin = NULL;
        int ret = 0;
        char tmp[8] = { 0 };
        uint32_t file_count= 0;
        size_t i = 0;
        uint32_t *file_sizes = NULL;

        fin = fopen(mergedfile, "rb");
        if (!fin) {
                printf("Unable to open %s\n", mergedfile);
                goto error;
        }

        if (fread(tmp, sizeof(tmp), 1, fin) != 1) {
                printf("Failed to read magic num\n");
                ret = -1;
                goto error;
        }

        if (memcmp(tmp, g_magic, sizeof(tmp)) != 0) {
                printf("Invalid kmerge file\n");
                ret = -1;
                goto error;
        }

        if (fread(&file_count, sizeof(file_count), 1, fin) != 1) {
                printf("Failed to read file count\n");
                ret = -1;
                goto error;
        }

        file_sizes = (uint32_t *)malloc(file_count * sizeof(uint32_t));
        if (!file_sizes) {
                printf("Failed to alloc %zu bytes to store file_sizes\n", file_count * sizeof(uint32_t));
                ret = -1;
                goto error;
        }

        if (fread(file_sizes, sizeof(uint32_t), file_count, fin) != file_count) {
                printf("Failed to read file sizes\n");
                ret = -1;
                goto error;
        }

        for (i = 0; i < file_count; ++i) {
                void *buf = NULL;
                char outfilename[16];
                sprintf(outfilename, "k%zu.bin", i+1);

                FILE *fout = fopen(outfilename, "wb");
                if (!fout) {
                        printf("Unable to open %s\n", outfilename);
                        ret = -1;
                        goto cleanup;
                }

                if (file_sizes[i] > 0) {
                        buf = malloc(file_sizes[i]);
                        if (!buf) {
                                printf("Failed to alloc %u bytes to save to %s\n", file_sizes[i], outfilename);
                                ret = -1;
                                goto cleanup;
                        }

                        if (fread(buf, file_sizes[i], 1, fin) != 1) {
                                printf("Failed to read %u bytes from %s\n", file_sizes[i], mergedfile);
                                ret = -1;
                                goto cleanup;
                        }
                        fwrite(buf, file_sizes[i], 1, fout);
                }
cleanup:
                if (buf) {
                        free(buf);
                }
                if (fout) {
                        fclose(fout);
                }
                if (ret < 0)
                        break;
        }


error:
        if (file_sizes)
                free(file_sizes);
        if (fin)
                fclose(fin);

        return ret;
}


int main(int argc, char *argv[])
{
        int ret = 0;

	if (argc == 2) {
                ret = ksplit(argv[1]);
        } else if (argc > 2) {
                ret = kmerge(argv[1], argv + 2, argc - 2);
        } else {
                printf(
                        "*** kmerge version 1 ***\n"
                        "Usage: kmerge mergedfile [file1] [file2] ... [fileN]\n"
                        "e.g. To merge x, y and z into m:\n"
                        "     kmerge m x y z\n"
                        "e.g. To split m:\n"
                        "     kmerge m\n"
                        "     k1.bin, k2.bin, ..., kN.bin will be generated from m\n");
        }

        return 0;
}
