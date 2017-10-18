#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/md5.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

typedef struct {
    char *name;
    char *fullpath;
    char *md5;
    int mtime;
    int size;
} File;


typedef struct {
    File **files;
    int size;
    int length;
} FileList;


typedef struct {
    char *key;
    FileList *value;
} Bucket;


typedef struct {
    Bucket **buckets;
    int size;
    int length;
} HashMap;


// converts unsigned char* md5 to char* for convenience
char* stringify_md5(unsigned char* md5) {
    int i;
    char *string = malloc(MD5_DIGEST_LENGTH + 1);
    char *string_ptr = string;
    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        snprintf(string_ptr, 2, "%02X ", md5[i]);
        string_ptr++;
    }
    return string;
}


// hashes string to md5
char* string_to_md5(char* string) {
    MD5_CTX c;
    unsigned char* md5 = malloc(MD5_DIGEST_LENGTH);

    MD5_Init(&c);
    MD5_Update(&c, string, strlen(string));
    MD5_Final(md5, &c);

    char* md5_string = stringify_md5(md5);

    free(md5);

    return md5_string;
}


// creates a new file struct
File *new_file(char *name, char *fullpath, int mtime, int size) {
    File *file = malloc(sizeof(File));

    file->name = malloc(strlen(name) + 1);
    strcpy(file->name, name);

    file->fullpath = malloc(strlen(fullpath) + 1);
    strcpy(file->fullpath, fullpath);

    file->md5 = NULL;

    file->mtime = mtime;

    file->size = size;

    return file;
}


// creates a new hashmap struct
HashMap *new_hashmap(int initial_size) {
    HashMap *map = malloc(sizeof(HashMap));
    map->buckets = malloc(sizeof(Bucket *) * initial_size);
    map->size = initial_size;
    map->length = 0;
    return map;
}


// creates a new file list struct
FileList *new_filelist(int initial_size) {
    FileList *list = malloc(sizeof(FileList));
    list->files = malloc(sizeof(File *) * initial_size);
    list->size = initial_size;
    list->length = 0;
    return list;
}


// creates a new bucket with key for hashmap
Bucket *new_bucket(char *key) {
    char* md5 = string_to_md5(key);
    Bucket *bucket = malloc(sizeof(Bucket));
    bucket->key = md5;
    bucket->value = new_filelist(2);
    return bucket;
}


// adds a file to file list
void add_to_filelist(FileList *list, File *file) {
    if (list->size == list->length) {
        list->size *= 2;
        list->files = realloc(list->files, (size_t) list->size * sizeof(File *));
    }

    list->files[list->length] = file;
    list->length++;
}


// adds a file to bucket
void add_to_bucket(Bucket *bucket, File *file) {
    return add_to_filelist(bucket->value, file);
}


// adds a bucket to hashmap
void add_to_hashmap(HashMap *hashmap, Bucket *bucket) {
    if (hashmap->size == hashmap->length) {
        hashmap->size *= 2;
        hashmap->buckets = realloc(hashmap->buckets, (size_t) hashmap->size * sizeof(Bucket *));
    }

    hashmap->buckets[hashmap->length] = bucket;
    hashmap->length++;
}


// finds a bucket in a hashmap by key
Bucket *get_bucket(HashMap *hashmap, char *key) {
    char* md5 = string_to_md5(key);
    int i;
    Bucket *current_bucket;
    for (i = 0; i < hashmap->length; ++i) {
        current_bucket = hashmap->buckets[i];
        if (strcmp(md5, current_bucket->key) == 0) {
            return current_bucket;
        }
    }
    return NULL;
}


// calculates file content MD5 hash
void calculate_file_md5(File *file) {

    MD5_CTX c;
    char buf[512];
    ssize_t bytes;
    unsigned char* md5 = malloc(MD5_DIGEST_LENGTH);

    int file_d = open(file->fullpath, O_RDONLY);
    if(file_d < 0) {
        printf("%s\n", file->fullpath);
        printf("could not open file\n%s\n", strerror(errno));
    }

    MD5_Init(&c);

    bytes = read(file_d, buf, 512);
    while (bytes > 0) {
        MD5_Update(&c, buf, bytes);
        bytes = read(file_d, buf, 512);
    }

    MD5_Final(md5, &c);

    char* md5_string = stringify_md5(md5);

    free(md5);

    file->md5 = md5_string;
}


// inserts a file in hashmap, creates a key depending on program parameters
Bucket *bucketize(HashMap *hashmap, File *file, int mtime_mode, int md5_mode) {
    char size[12];
    sprintf(size, "%d", file->size);
    int keylen = (int)strlen(file->name) + (int)strlen(size);
    char mtime[12];
    if (mtime_mode && md5_mode) {
        sprintf(mtime, "%d", file->mtime);
        keylen += (int)strlen(mtime);
        keylen += (int)strlen(file->md5);
    } else if (mtime_mode) {
        sprintf(mtime, "%d", file->mtime);
        keylen += (int)strlen(mtime);
    } else if (md5_mode) {
        keylen = (int)strlen(file->md5);
    }

    char* key = malloc((size_t)keylen + 1);

    if (mtime_mode && md5_mode) {
        strcpy(key, file->name);
        strcat(key, size);
        strcat(key, mtime);
        strcat(key, file->md5);
    } else if (mtime_mode) {
        strcpy(key, file->name);
        strcat(key, size);
        strcat(key, mtime);
    } else if (md5_mode) {
        strcat(key, file->md5);
    } else {
        strcpy(key, file->name);
        strcat(key, size);
    }

    Bucket *bucket = get_bucket(hashmap, key);
    if (!bucket) {
        bucket = new_bucket(key);
        add_to_hashmap(hashmap, bucket);
    }
    add_to_bucket(bucket, file);
    return bucket;
}


// recursive function for traversing the file tree and looking for duplicate files
void duplicate_search(char *path, HashMap *filemap, int mtime_mode, int md5_mode) {
    DIR *directory = opendir(path);
    struct dirent *entry;

    if (directory == NULL) {
        printf("could not open directory\n");
        perror("opendir");
    }

    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            int path_length = (int) strlen(path) + (int) strlen(entry->d_name) + 1;
            int needs_slash = 0;
            if (path[(int) strlen(path) - 1] != '/') {
                path_length++;
                needs_slash = 1;
            }

            char *subPath = malloc((size_t) path_length);

            strcpy(subPath, path);
            if (needs_slash) {
                strcat(subPath, "/");
            }
            strcat(subPath, entry->d_name);

            if (entry->d_type == DT_DIR) {
                duplicate_search(subPath, filemap, mtime_mode, md5_mode);
            } else if (entry->d_type != DT_LNK) {
                struct stat *file_stat = malloc(sizeof(struct stat));
                stat(subPath, file_stat);
                File *file = new_file(entry->d_name, subPath, (int) file_stat->st_mtime, (int) file_stat->st_size);
                if (md5_mode) {
                    calculate_file_md5(file);
                }
                bucketize(filemap, file, mtime_mode, md5_mode);
            }
            free(subPath);
        }
    }
    closedir(directory);
}


// formats the date
char* format_date(time_t val) {
    char* time_string = malloc(36);
    strftime(time_string, 36, "%d-%m-%Y %H:%M", localtime(&val));
    return time_string;
}


// prints duplicate file buckets from hashmap
void print(HashMap *hashmap) {
    int i;
    int j;
    for (i = 0; i < hashmap->length; ++i) {
        Bucket *bucket = hashmap->buckets[i];
        if (bucket->value->length > 1) {
            File* original = bucket->value->files[0];
            char* md5;
            if (original->md5) {
                md5 = original->md5;
            } else {
                md5 = malloc(1);
                strcpy(md5, "");
            }
            printf("=== %s %d %s %s\n", format_date(original->mtime), original->size, original->name, md5);
            for (j = 0; j < bucket->value->length; ++j) {
                printf("%s\n", bucket->value->files[j]->fullpath + 2);
            }
            printf("\n");
        }
    }
}


// prints the help text
void help(char* program_name) {
    printf("Utility for duplicate file lookup in the current working directory.\n");
    printf("By default looks for duplicate files by name and size.\n");
    printf("usage: %s [-m] [-d] [-h]\n", program_name);
    printf("Detailed description of flag usage:\n");
    printf("    -m    Uses modified time in conjunction with name and size for duplicate comparison.\n");
    printf("    -d    Uses only MD5 content hash for duplicate comparison.\n");
    printf("    -d -m   Uses modified time and MD5 content hash in conjunction with name and size for duplicate comparison.\n");
    printf("    -h    Displays this help text.\n");
}


int main(int argc, char **argv) {

    int mtime_mode = 0;
    int md5_mode = 0;

    int i;
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-d") == 0) {
            mtime_mode = 1;
        } else if (strcmp(argv[i], "-m") == 0) {
            md5_mode = 1;
        } else if (strcmp(argv[i], "-h") == 0) {
            help(argv[0]);
            return 0;
        } else {
            printf("%s\n", "invalid argument");
            return 1;
        }
    }

    HashMap *filemap = new_hashmap(10);

    duplicate_search(".", filemap, mtime_mode, md5_mode);

    print(filemap);

    return 0;
}

