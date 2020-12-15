#include "jsmn.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/*
 * 'slurp' reads the file identified by 'path' into a character buffer
 * pointed at by 'buf', optionally adding a terminating NUL if
 * 'add_nul' is true. On success, the size of the file is returned; on
 * failure, -1 is returned and ERRNO is set by the underlying system
 * or library call that failed.
 *
 * WARNING: 'slurp' malloc()s memory to '*buf' which must be freed by
 * the caller.
 */
long slurp(char const* path, char **buf, bool add_nul)
{
    FILE  *fp;
    size_t fsz;
    long   off_end;
    int    rc;

    /* Open the file */
    fp = fopen(path, "rb");
    if( NULL == fp ) {
        return -1L;
    }

    /* Seek to the end of the file */
    rc = fseek(fp, 0L, SEEK_END);
    if( 0 != rc ) {
        return -1L;
    }

    /* Byte offset to the end of the file (size) */
    if( 0 > (off_end = ftell(fp)) ) {
        return -1L;
    }
    fsz = (size_t)off_end;

    /* Allocate a buffer to hold the whole file */
    *buf = malloc( fsz+(int)add_nul );
    if( NULL == *buf ) {
        return -1L;
    }

    /* Rewind file pointer to start of file */
    rewind(fp);

    /* Slurp file into buffer */
    if( fsz != fread(*buf, 1, fsz, fp) ) {
        free(*buf);
        return -1L;
    }

    /* Close the file */
    if( EOF == fclose(fp) ) {
        free(*buf);
        return -1L;
    }

    if( add_nul ) {
        /* Make sure the buffer is NUL-terminated, just in case */
        buf[fsz] = 0;//'\0';
    }

    /* Return the file size */
    return (long)fsz;
}


/*
 * Usage message for demo (in main(), below)
 */
void usage(void) {
    fputs("USAGE: ./slurp <filename>\n", stderr);
    exit(1);
}

/*
 * A small example of jsmn parsing when JSON structure is known and number of
 * tokens is predictable.
 */

// static const char *JSON_STRING =
//     "{\"num_angles\": 2, \"num_markers\": 3,\n  "
//     "\"marker_start\": [[240, 64, 33], [241, 65, 34]],\n"
//     "\"marker_mid\": [[340, 74, 43], [341, 75, 44]],\n"
//     "\"marker_end\": [[440, 84, 53], [441, 85, 54]],\n"
//     "}";

static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

// Parse JSON file
int parse_json(int **marker_color, int **marker_start, int **marker_mid, int **marker_end, int *num_ang, int* num_mark, int **angles) {
  int i;
  int r;
  jsmn_parser p;
  jsmntok_t t[128]; /* We expect no more than 128 tokens */
  long  file_size;
  char *buf;

  /* Try the first command-line argument as a file name */
  file_size = slurp("marker.json", &buf, false);

  /* Bail if we get a negative file size back from slurp() */
  if( file_size < 0L ) {
      perror("File read failed");
      usage();
  }

  printf("File %s", buf);

  jsmn_init(&p);
  r = jsmn_parse(&p, buf, file_size, t,
                 sizeof(t) / sizeof(t[0]));

  if (r < 0) {
    printf("Failed to parse JSON: %d\n", r);
    return 1;
  }

  /* Assume the top-level element is an object */
  if (r < 1 || t[0].type != JSMN_OBJECT) {
    printf("Object expected\n");
    return 1;
  }

  /* get dimensions */
  int num_angles = 0;
  int num_markers = 0;

  /* Loop over all keys of the root object */
  for (i = 1; i < r; i++) {
    //printf("Next token: %.*s\n", t[i].end - t[i].start, buf + t[i].start);
    if (jsoneq(buf, &t[i], "num_angles") == 0) {
      /* We may use strndup() to fetch string value */
      num_angles = atoi(buf + t[i + 1].start);

      i++;
    } else if (jsoneq(buf, &t[i], "num_markers") == 0) {
      /* We may additionally check if the value is either "true" or "false" */
      num_markers = atoi(buf + t[i + 1].start);

      i++;
    }
  }

  /* print and test dimensions */
  printf("Dimensions, num_angles %d, num_markers %d\n", num_angles, num_markers);

  if(num_angles == 0 || num_markers == 0){
    printf("Dimension error!\n");
    return EXIT_FAILURE;
  }

  // transfer dimentions
  *num_ang = num_angles;
  *num_mark = num_markers;

  /* allocate */
  *marker_start = (int*) malloc(num_angles * num_markers * 2 * sizeof(int));
  *marker_mid = (int*) malloc(num_angles * num_markers * 2 * sizeof(int));
  *marker_end = (int*) malloc(num_angles * num_markers * 2 * sizeof(int));

  // angles
  *angles = (int*) malloc(num_angles * sizeof(int));

  // colors
  *marker_color = (int*) malloc(num_markers * sizeof(int));

  // allocated by malloc or not 
  if (*marker_start == NULL || *marker_mid == NULL || *marker_end == NULL || *angles == NULL) { 
      printf("Memory not allocated.\n"); 
      return EXIT_FAILURE; 
  } 

  /* Loop over all keys of the root object */
  for (i = 1; i < r; i++) {
    //printf("Next token: %.*s\n", t[i].end - t[i].start, buf + t[i].start);
    if (jsoneq(buf, &t[i], "angles") == 0) {
      int indx = i + 1;
      if (t[indx].type != JSMN_ARRAY) {
        continue; /* We expect angles to be an array of strings */
      }

      // test first dimention matches angles
      int sz_arr = t[indx++].size;

      if(sz_arr != num_angles){
        printf("Number of angles %d does not match angles %d!\n", sz_arr, num_angles);
        return EXIT_FAILURE;
      }

      // loop over angles
      int j;
      for (j = 0; j < sz_arr; j++) {
        jsmntok_t *gi = &t[indx++];
        int x = atoi(buf + gi->start);
        //printf("  ang* %.*s, %d\n", gi->end - gi->start, buf + gi->start, x);
        (*angles)[j] = x;
      }

      i = indx - 1;

    } else if (jsoneq(buf, &t[i], "marker_color") == 0) {
      int indx = i + 1;
      if (t[indx].type != JSMN_ARRAY) {
        continue; /* We expect colors to be an array of strings */
      }

      // test first dimention matches angles
      int sz_arr = t[indx++].size;

      if(sz_arr != num_markers){
        printf("Number of colors %d does not match num_markers %d!\n", sz_arr, num_markers);
        return EXIT_FAILURE;
      }

      // loop over colors
      int j;
      for (j = 0; j < sz_arr; j++) {
        jsmntok_t *gi = &t[indx++];
        int x = atoi(buf + gi->start);
        //printf("  ang* %.*s, %d\n", gi->end - gi->start, buf + gi->start, x);
        (*marker_color)[j] = x;
      }

      i = indx - 1;

    /* section for location arrays */
    } else if (jsoneq(buf, &t[i], "marker_start") == 0 ||
                jsoneq(buf, &t[i], "marker_mid") == 0 ||
                  jsoneq(buf, &t[i], "marker_end") == 0 ) {

      int* ptr = NULL;
      
      if (jsoneq(buf, &t[i], "marker_start") == 0){
        ptr = *marker_start;
        //printf("marker_start:\n");
      }else if(jsoneq(buf, &t[i], "marker_mid") == 0){
        ptr = *marker_mid;
        //printf("marker_mid:\n");
      }else if(jsoneq(buf, &t[i], "marker_end") == 0){
        ptr = *marker_end;
        //printf("marker_end:\n");
      }

      //setup index to track tokens
      int indx = i + 1;
      
      if (t[indx].type != JSMN_ARRAY) {
        continue; /* We expect groups to be an array of strings */
      }

      // test first dimention matches angles
      int sz_arr = t[indx++].size;

      if(sz_arr != num_angles){
        printf("First dimention %d does not match angles %d!\n", sz_arr, num_angles);
        return EXIT_FAILURE;
      }
      //printf("outer array! %d\n", sz_arr);

      // loop over angles
      int j;
      for (j = 0; j < sz_arr; j++) {
        //printf("array J %d\n", j);
        jsmntok_t *g = &t[indx++];

        // test it is an array
        if (g->type == JSMN_ARRAY) {
          //printf("array! %d\n", g->size);
          // test second dimention matches num_markers
          int sz_arr_inner = g->size;
          if(sz_arr_inner != num_markers * 2){
            printf("Second dimention %d does not match markers %d!\n", sz_arr_inner, num_markers);
            return EXIT_FAILURE;
          }

          // loop over markers
          int k;
          for (k = 0; k < sz_arr_inner; k++) {
            jsmntok_t *gi = &t[indx++];
            int x = atoi(buf + gi->start);
            //printf("  * %.*s, %d\n", gi->end - gi->start, buf + gi->start, x);
            ptr[k * num_angles + j] = x;
          }
        }
      }
      // fix i for next
      i = indx - 1;
    } else {
      printf("Unexpected key: %.*s\n", t[i].end - t[i].start,
             buf + t[i].start);
    }
  }

  // dealloc buf
  free(buf);

  return EXIT_SUCCESS;
}
