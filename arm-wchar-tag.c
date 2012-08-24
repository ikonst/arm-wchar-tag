/*
 * arm-wchar-tag.c
 *
 * This utility displays Tag_ABI_PCS_wchar_t value of
 * ARM EABI ELF files (which can also be done with
 * 'readelf -a') and also allows to patch it, most
 * commonly with a '0', to indicate this ELF file
 * is wchar_t-agnostic.
 *
 * For documentation, see:
 * http://infocenter.arm.com/help/topic/com.arm.doc.ihi0045c/IHI0045C_ABI_addenda.pdf
 * [ Addenda to, and Errata in, the ABI for the ARM® Architecture ]
 *
 * This code is in the public domain.
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <elf.h>

/* Reads an ULEB128 (variable-length integer) value from the file descriptor.

   'pos' is the current position within the section and 'size' is the
   size of the section. The function will take care not to run outside
   of the section. */
int parse_uleb128(int fd, unsigned long int* result, off_t* pos, size_t size)
{
  int               shift = 0;
  unsigned char     byte;
  
  *result = 0;

  while (*pos < size)
  {
    if (read(fd, &byte, sizeof(byte)) != 1)
    {
      perror("reading ULEB128");
      return 1;
    }
    ++(*pos);
    
    *result |= (byte & 0x7f) << shift;
    if ((byte & 0x80) == 0)
      break;
      
    if (*pos >= size)
    {
      printf("Error: Unterminated ULEB128.\n");
      return 1;
    }

    shift += 7;
  }
  
  return 0;
}

/* Reads an NTBS (null-terminated string) value from the file descriptor.

   'pos' is the current position within the section and 'size' is the
   size of the section. The function will take care not to run outside
   of the section. */
int parse_ntbs(int fd, char* result, size_t result_size, off_t* pos, size_t size)
{
  off_t i = 0;

  while (*pos < size)
  {
    char byte;
    if (read(fd, &byte, sizeof(byte)) != 1)
    {
      perror("skipping NTBS\n");
      return 1;
    }
    ++(*pos);
    
    if ((result != NULL) && (i < result_size))
    {
      result[i] = byte;
    }
    ++i;
    
    if (byte == 0)
      break;
      
    if (*pos >= size)
    {
      printf("Error: Unterminated NTBS.\n");
      return 1;
    }
  }
  
  // ensure null-termination
  if (result != NULL)
  {
    result[result_size-1] = '\0';
  }
  
  return 0;
}

/* Parses the ARM attributes section's 'eabi' subsection */
int parse_eabi_attr_aeabi_subsection(int fd, off_t* pos, size_t sh_size, char wchar_size)
{
  unsigned long int attr, value;
  int ret;
  char buf[1024];
  
  if (sh_size < 1)
  {
    printf("Error: aeabi subsection too small.\n");
    return 1;
  }
  
  unsigned char tag;
  if (read(fd, &tag, sizeof(tag)) != sizeof(tag))
  {
    perror("reading aeabi subsection tag\n");
    return 1;
  }
  ++(*pos);
  
  // if tag = section or tag = symbol, skip over section/symbol identifiers
  if (tag == 2 || tag == 3)
  {
    unsigned long int id;
    do
    {
      ret = parse_uleb128(fd, &id, pos, sh_size);
      if (ret != 0)
        return ret;
    } while (id != 0);    
  }
  
  while (*pos < sh_size)
  {
    ret = parse_uleb128(fd, &attr, pos, sh_size);
    if (ret != 0)
      return ret;
      
    switch (attr)
    {
      case 4: // Tag_CPU_raw_name 
      case 5: // Tag_CPU_name 
      case 67: // Tag_conformance 
      case 32: // Tag_compatibility
        ret = parse_ntbs(fd, buf, sizeof(buf), pos, sh_size);
        if (ret != 0)
          return ret;
        break;
      case 18: // Tag_ABI_PCS_wchar_t
        ret = parse_uleb128(fd, &value, pos, sh_size);
        if (ret != 0)
          return ret;
        printf("Tag_ABI_PCS_wchar_t = %ld", value);
        if (wchar_size >= 0)
        {
          if (value > 0x7f)
          {
            // This utility does not support resizing structures
            printf("Error: Unable to patch Tag_ABI_PCS_wchar_t: old value is too big.\n");
          }
          if (lseek(fd, -1, SEEK_CUR) == (off_t)-1)
          {
            perror("seeking to patch");
            return 1;
          }
          if (write(fd, &wchar_size, sizeof(wchar_size)) != sizeof(wchar_size))
          {
            perror("patching");
            return 1;
          }
          printf(", patched to %d\n", wchar_size);
        }
        else
        {
          printf("\n");
        }
        break;
      default:
        // skip over tag -- for >32, we follow ARM's convention
        if (attr > 32)
        {
          if ((attr % 2) == 0) // even
            ret = parse_uleb128(fd, &value, pos, sh_size);
          else // odd
            ret = parse_ntbs(fd, NULL, 0, pos, sh_size);
        }
        else
        {
          // all NTBS tags are in the switch above; the rest are ULEB128
          ret = parse_uleb128(fd, &value, pos, sh_size);
        }
        if (ret != 0)
          return ret;
        break;
    }
  }
  
  return 0;
}

/* Parses the ARM attributes ELF section */
int parse_eabi_attr_section(int fd, size_t sh_size, char wchar_size)
{
  int ret;
  
  if (sh_size < 1)
  {
    printf("Error: Empty ARM attributes section.\n");
  }

  char version;
  if (read(fd, &version, sizeof(version)) != sizeof(version))
  {
    perror("reading version");
    return 1;
  }
  
  if (version != 'A')
  {
    printf("Error: Unknown ARM attribute section format version '%c'.\n", version);
    return 1;
  }
  
  off_t pos = 1;
  
  while (pos < sh_size)
  {
    Elf32_Word subsect_size;
    if (pos + sizeof(subsect_size) > sh_size)
    {
      printf("Error: Unexpected end of ARM attribute section\n");
      return 1;
    }
    if (read(fd, &subsect_size, sizeof(subsect_size)) != sizeof(subsect_size))
    {
      perror("reading subsection size");
      return 1;
    }
    if (pos + subsect_size > sh_size)
    {
      printf("Error: ARM attribute subsection outside of section bounds.\n");
      return 1;
    }
    
    off_t spos = sizeof(subsect_size); // positon within subsection
    
    char vendor_name[128];
    ret = parse_ntbs(fd, vendor_name, sizeof(vendor_name), &spos, subsect_size);
    if (ret != 0)
      return ret;
      
    if (strcmp(vendor_name, "aeabi") == 0)
    {
      ret = parse_eabi_attr_aeabi_subsection(fd, &spos, subsect_size, wchar_size);
      if (ret != 0)
        return ret;
    }
    else
    {
      if (lseek(fd, subsect_size - spos, SEEK_CUR) != -1)
      {
        perror("skipping over unknown subsection");
        return 1;
      }
      
      spos = subsect_size;
    }
      
    pos += spos;
  }
  
  return 0;
}

/* Parses the ELF file */
int parse(int fd, char wchar_size)
{
  Elf32_Ehdr ehdr;
  if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
  {
    perror("reading ELf32_Ehdr");
    return 1;
  }
  
  if (memcmp(ehdr.e_ident, ELFMAG, 4) != 0)
  {
    printf("Error: Invalid ELF magic.\n");
    return 1;
  }

  // Real-world ARM EABI files don't have this, for some reason
#ifdef IDENT_HAS_EABI  
  if (ehdr.e_ident[EI_OSABI] != 64)
  {
    printf("Error: Not ARM EABI file.\n");
    return 1;
  }
#endif
  
  if (ehdr.e_machine != EM_ARM)
  {
    printf("Error: Not an ARM ELF file.\n");
    return 1;
  }
  
  if (ehdr.e_shoff == 0)
  {
    printf("Error: ELF file has no section table.\n");
    return 1;
  }
  
  if (ehdr.e_shentsize != sizeof(Elf32_Shdr))
  {
    printf("Error: Section header entry size %d doesn't match sizeof(ELf32_Shdr)=%d.\n", ehdr.e_shentsize, sizeof(Elf32_Shdr));
    return 1;
  }
  
  if (lseek(fd, ehdr.e_shoff, SEEK_SET) == (off_t)-1)
  {
    perror("seeking to section header table");
    return 1;
  }
  
  for (int i=0; i<ehdr.e_shnum; ++i)
  {
    Elf32_Shdr shdr;
    if (read(fd, &shdr, sizeof(shdr)) != sizeof(shdr))
    {
      perror("reading Elf32_Shdr");
      return 1;
    }
    
    if (shdr.sh_type != SHT_ARM_ATTRIBUTES)
      continue;
      
    off_t current_pos = lseek(fd, 0, SEEK_CUR);
      
    if (lseek(fd, shdr.sh_offset, SEEK_SET) == (off_t)-1)
    {
      perror("seeking to attributes section");
      return 1;
    }
    
    int ret = parse_eabi_attr_section(fd, shdr.sh_size, wchar_size);
    if (ret != 0)
      return ret;
    
    if (lseek(fd, current_pos, SEEK_SET) == (off_t)-1)
    {
      perror("restoring position");
      return 1;
    }
  }
  
  return 0;
}

int process(const char* filename, int wchar_size)
{
  int fd = open(filename, O_RDWR);
  if (fd == -1)
  {
    perror("opening file");
    return 1;
  }
  
  int ret = parse(fd, wchar_size);
  
  close(fd);
  
  return ret;
}

int main(int argc, const char** argv)
{
  if (argc < 2 || argc > 3)
  {
    printf("Syntax: arm-wchar-tag [filename] [Tag_ABI_PCS_wchar_t]\n");
    return 1;
  }
  
  int wchar_size;
  if (argc == 3)
  {
    if (sscanf(argv[2], "%d", &wchar_size) != 1)
    {
      printf("Invalid Tag_ABI_PCS_wchar_t value %s.\n", argv[2]);
      return 1;
    }
  
    if (wchar_size > 0x7f)
    {
      printf("Error: We do not support patching with TAG_ABI_PCS_wchar_t %d greater than 0x7f.\n", wchar_size);
      return 1;
    }
  }
  else
  {
    wchar_size = -1;
  }

  return process(argv[1], (char)wchar_size);
}
