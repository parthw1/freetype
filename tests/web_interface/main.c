// 'main' takes in .so files for two versions of FT, the character size, and
// a font directory. It uses 'bitmap' and 'murmur3' to compare hashes for all
// glyphs in the font, using the base and test FT libraries, then renders
// differing glyphs as PNGs. A webpage is generated with a table of these 
// glyphs.
#include "bitmap.h"
#include <dlfcn.h> // for dynamic linking
#include <math.h> // for glyph comparison

// a single 'entry' is related to a single charcode for a glyph, as well as 
// a row in the generated html table
struct entry 
{
  char base_img[256]; // filename of base png
  char test_img[256]; // filename of test png
  char base_hash[33]; // base mm3 hash
  char test_hash[33]; // test mm3 hash
  double base_value; // comparison metric for base glyph
  double test_value; // comparison metric for test glyph
  double difference; // difference holding the distance between glyphs
};

void render(const char*, const char*, FT_UInt32, int, struct entry**, int*);

int compare(const void* e1, const void* e2);

void make_html(struct entry**, int* , const char*);

int main(int argc, char const *argv[])
{
  const char* base_ft; // path of base ft
  const char* test_ft; // path of test ft
  FT_UInt32 size; // char size
  const char* font; // path of font
  int mode;

  // create array of structs with size 1
  struct entry* entries = malloc(1 * sizeof(struct entry));
  int entries_len = 1;

  if(argc != 5)
  {
    printf("Usage: %s <base ft.so> <test ft.so> <char size> <font>\n", argv[0]);
    return 0;
  }

  base_ft = argv[1];
  test_ft = argv[2];
  size = atoi(argv[3]);
  font = argv[4];

  mode = 0; // base hashes
  render(base_ft, font, size, mode, &entries, &entries_len);

  mode = 1; // test hashes
  render(test_ft, font, size, mode, &entries, &entries_len);

  mode = 2; // base images for differing glyphs
  render(test_ft, font, size, mode, &entries, &entries_len);
  
  mode = 3; // test images for differing glyphs
  render(test_ft, font, size, mode, &entries, &entries_len);

  // quicksort array by decreasing difference
  qsort(entries, entries_len, sizeof(struct entry), compare);

  // generate HTML output
  make_html(&entries, &entries_len, font);

  // free array
  free(entries);

}

// generate HTML output 
void make_html(struct entry** entries, int* num, const char* font)
{
  FILE *fp = fopen("index.html", "w");
  if (fp == NULL)
  {
    printf("Error opening file.\n");
    exit(1);
  }
  
  // TODO: refactor
  fprintf(fp, "<!DOCTYPE html>\n<html>\n<head>\n<style>\nimg{image-rendering: optimizeSpeed;image-rendering: -moz-crisp-edges;image-rendering: -o-crisp-edges;image-rendering: -webkit-optimize-contrast;image-rendering: pixelated;image-rendering: optimize-contrast;-ms-interpolation-mode: nearest-neighbor;min-width:10%%}\ntable, th, td{\nborder: 1px solid black;\n}\n</style>\n</head>\n\n<body>\n");
  fprintf(fp, "<p>%s</p>\n<table style=\"width:100%%\">", font);
  fprintf(fp, "<tr><th>ID</th><th>Difference</th><th>Base glyph | Test glyph</th></tr>\n");
  for (int i = 0; i < *num; ++i)
  {
    if ((*entries)[i].difference > 0){
      fprintf(fp, "<tr><td>%d</td><td>%.2f</td><td><img src=\"%s\"</img> <img src=\"%s\"</img></td></tr>\n", i, (*entries)[i].difference, (*entries)[i].base_img, (*entries)[i].test_img);
    }
  }
  fprintf(fp, "</table>\n</body>\n</html>");
  fclose(fp);

}

// comparison function for quick sort 
int compare (const void* e1, const void* e2)
{
  struct entry *s1 = (struct entry *)e1;
  struct entry *s2 = (struct entry *)e2;
  int comp = (int)(s2->difference) - (int)(s1->difference);
  return comp;
}

// multifunctional render function
void render(const char* ft_dir, const char* font, FT_UInt32 size, int mode, struct entry** entries, int* entries_len)
{
  FT_Library library;
  FT_Face face;
  FT_GlyphSlot slot;
  FT_Bitmap *bitmap;
  FT_Error error;  
  int i;

  // declare dynamically loaded functions 
  FT_Error (*ft_init_fun)(FT_Library*);
  FT_Error (*ft_newface_fun)(FT_Library, const char*, FT_Long, FT_Face*);
  FT_Error (*ft_setcharsize_fun)(FT_Face, FT_F26Dot6, FT_F26Dot6, FT_UInt, 
    FT_UInt);
  FT_Error (*ft_loadglyph_fun)(FT_Face, FT_UInt, FT_Int32);
  FT_Error (*ft_renderglyph_fun)(FT_GlyphSlot, FT_Render_Mode);
  FT_Error (*ft_doneface_fun)(FT_Face);
  FT_Error (*ft_donefreetype_fun)(FT_Library);
  void (*ft_bitmapinit_fun)(FT_Bitmap*);
  FT_Error (*ft_bitmapconvert_fun)(FT_Library, const FT_Bitmap*, FT_Bitmap, 
    FT_Int);

  // dynamically load ft version specified by ft_dir
  void* handle = dlopen(ft_dir, RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);
  if (!handle) {
    fputs(dlerror(), stderr);
    exit(1);
  }

  // error checking
  dlerror();

  // initialize dynamically loaded functions
  *(void**)(&ft_init_fun) = dlsym(handle,"FT_Init_FreeType");
  *(void**)(&ft_newface_fun) = dlsym(handle,"FT_New_Face");
  *(void**)(&ft_setcharsize_fun) = dlsym(handle,"FT_Set_Char_Size");
  *(void**)(&ft_loadglyph_fun) = dlsym(handle,"FT_Load_Glyph");
  *(void**)(&ft_renderglyph_fun) = dlsym(handle,"FT_Render_Glyph");
  *(void**)(&ft_doneface_fun) = dlsym(handle,"FT_Done_Face");
  *(void**)(&ft_donefreetype_fun) = dlsym(handle,"FT_Done_FreeType");
  *(void**)(&ft_bitmapinit_fun) = dlsym(handle,"FT_Bitmap_Init");
  *(void**)(&ft_bitmapconvert_fun) = dlsym(handle,"FT_Bitmap_Convert");

  // error checking
  dlerror();

  error = ft_init_fun(&library);
  if (error) 
  {
    printf("Error: library init");
  }

  error = ft_newface_fun(library, font, 0, &face);
  if (error) 
  {
    printf("Error: loading face");
  }

  error = ft_setcharsize_fun(face, size * 64, 0, 96, 0);
  if (error) 
  {
    printf("Error: setting char size");
  }

  slot = face->glyph;

  // if 'images' folder doesn't exist, create it
  struct stat st = {0};

  if (stat("./images/", &st) == -1) {
      mkdir("./images/", 0777);
  }

  // iterate over all glyphs in the font
  for (i = 0; i < face->num_glyphs; ++i)
  {
    // if we've reached capacity of array, double array size
    if (i == *entries_len)
    {
      *entries = realloc(*entries, (*entries_len * 2) * sizeof (struct entry));
      entries = &(*entries);
      *entries_len = *entries_len * 2;
    }

    (*entries)[i].difference = 0.0; // initially assume no difference between glyphs

    // if hashes are the same, move to the next glyph
    if (((mode == 2) || (mode == 3)) && 
      (strcmp((*entries)[i].base_hash, (*entries)[i].test_hash) == 0))
    {
      continue;
    }  

    error = ft_loadglyph_fun(face, i, FT_LOAD_DEFAULT);
    if (error)
    {
      printf("Error: loading glyph");
    }

    error = ft_renderglyph_fun(slot, FT_RENDER_MODE_NORMAL);
    if (error)
    {
      printf("Error: rendering glyph");
    }

    bitmap = &slot->bitmap;

    // calculates and stores hash for a glyph
    if ((mode == 0) || (mode == 1))
    {
      HASH_128 * murmur = (HASH_128 *)malloc(sizeof(HASH_128));
      murmur = Generate_Hash_x64_128(bitmap, murmur);
      if (mode == 0)
      {
        sprintf((*entries)[i].base_hash, "%08x%08x%08x%08x", 
          murmur->hash[0], murmur->hash[1], murmur->hash[2], murmur->hash[3]);
      }
      else 
      {  
        sprintf((*entries)[i].test_hash, "%08x%08x%08x%08x", 
          murmur->hash[0], murmur->hash[1], murmur->hash[2], murmur->hash[3]);
  
      }

    } else { // if we're done hashing
      // if glyph is empty, move to the next glyph
      if (bitmap->width == 0 || bitmap->rows == 0)
      {
        continue;
      }

      // the glyphs have a different hash and are not empty so we generate a 
      // PNG for the glyph and calculate the difference between them
      if (mode == 2)
      {
        Make_PNG(bitmap, "./images/base", i, 1); 
        sprintf((*entries)[i].base_img, "images/base_%d.png", i);
        (*entries)[i].base_value = (double)(rand() % 1000);
      } else if (mode == 3){
        Make_PNG(bitmap, "./images/test", i, 1);
        sprintf((*entries)[i].test_img, "images/test_%d.png", i);
        (*entries)[i].test_value = (double)(rand() % 1000);
        (*entries)[i].difference = fabs((*entries)[i].base_value - (*entries)[i].test_value);
      }
    }
  }

  error = ft_doneface_fun(face);
  if (error)
  {
    printf("Error: freeing face");
  }

  error = ft_donefreetype_fun(library);
  if (error)
  {
    printf("Error: freeing library");
  }
  dlclose(handle);
}