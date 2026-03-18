/* BFD back-end for Zilog Z800n COFF binaries.
   Copyright (C) 1992-2026 Free Software Foundation, Inc.
   Contributed by Cygnus Support.
   Written by Steve Chamberlain, <sac@cygnus.com>.

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "bfdlink.h"
#include "genlink.h"
#include "coff/z8k.h"
#include "coff/internal.h"
#include "libcoff.h"

#define COFF_DEFAULT_SECTION_ALIGNMENT_POWER (1)

static reloc_howto_type r_imm32 =
HOWTO (R_IMM32, 0, 4, 32, false, 0,
       complain_overflow_bitfield, 0, "r_imm32", true, 0xffffffff,
       0xffffffff, false);

/* Internal relocation type used during linker relaxation.  When a segmented
   address (R_IMM32) has an offset that fits in 8 bits, we relax it to the
   short form, saving 2 bytes.  This type never appears in object files.
   We keep size=4 so the bounds check in extra_case passes (the source
   still has 4 bytes), but we only write 2 bytes to the output.  */
#define R_IMM32_SHORT 0x30
static reloc_howto_type r_imm32_short =
HOWTO (R_IMM32_SHORT, 0, 4, 32, false, 0,
       complain_overflow_bitfield, 0, "r_imm32_short", true, 0xffffffff,
       0xffffffff, false);

static reloc_howto_type r_imm4l =
HOWTO (R_IMM4L, 0, 1, 4, false, 0,
       complain_overflow_bitfield, 0, "r_imm4l", true, 0xf, 0xf, false);

static reloc_howto_type r_da =
HOWTO (R_IMM16, 0, 2, 16, false, 0,
       complain_overflow_bitfield, 0, "r_da", true, 0x0000ffff, 0x0000ffff,
       false);

static reloc_howto_type r_imm8 =
HOWTO (R_IMM8, 0, 1, 8, false, 0,
       complain_overflow_bitfield, 0, "r_imm8", true, 0x000000ff, 0x000000ff,
       false);

static reloc_howto_type r_rel16 =
HOWTO (R_REL16, 0, 2, 16, false, 0,
       complain_overflow_bitfield, 0, "r_rel16", true, 0x0000ffff, 0x0000ffff,
       true);

static reloc_howto_type r_jr =
HOWTO (R_JR, 1, 1, 8, true, 0, complain_overflow_signed, 0,
       "r_jr", true, 0xff, 0xff, true);

static reloc_howto_type r_disp7 =
HOWTO (R_DISP7, 0, 1, 7, true, 0, complain_overflow_bitfield, 0,
       "r_disp7", true, 0x7f, 0x7f, true);

static reloc_howto_type r_callr =
HOWTO (R_CALLR, 1, 2, 12, true, 0, complain_overflow_signed, 0,
       "r_callr", true, 0xfff, 0xfff, true);

#define BADMAG(x) Z8KBADMAG(x)
#define Z8K 1			/* Customize coffcode.h.  */
#define __A_MAGIC_SET__

/* Code to swap in the reloc.  */
#define SWAP_IN_RELOC_OFFSET	H_GET_32
#define SWAP_OUT_RELOC_OFFSET	H_PUT_32
#define SWAP_OUT_RELOC_EXTRA(abfd, src, dst) \
  dst->r_stuff[0] = 'S'; \
  dst->r_stuff[1] = 'C';

/* Code to turn a r_type into a howto ptr, uses the above howto table.  */

static void
rtype2howto (arelent *internal, struct internal_reloc *dst)
{
  switch (dst->r_type)
    {
    default:
      internal->howto = NULL;
      break;
    case R_IMM8:
      internal->howto = &r_imm8;
      break;
     case R_IMM16:
      internal->howto = &r_da;
      break;
    case R_JR:
      internal->howto = &r_jr;
      break;
    case R_DISP7:
      internal->howto = &r_disp7;
      break;
    case R_CALLR:
      internal->howto = &r_callr;
      break;
    case R_REL16:
      internal->howto = &r_rel16;
      break;
    case R_IMM32:
      internal->howto = &r_imm32;
      break;
    case R_IMM4L:
      internal->howto = &r_imm4l;
      break;
    }
}

#define RTYPE2HOWTO(internal, relocentry) rtype2howto (internal, relocentry)

static reloc_howto_type *
coff_z8k_reloc_type_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			    bfd_reloc_code_real_type code)
{
  switch (code)
    {
    case BFD_RELOC_8:		return & r_imm8;
    case BFD_RELOC_16:		return & r_da;
    case BFD_RELOC_32:		return & r_imm32;
    case BFD_RELOC_8_PCREL:	return & r_jr;
    case BFD_RELOC_16_PCREL:	return & r_rel16;
    case BFD_RELOC_Z8K_DISP7:	return & r_disp7;
    case BFD_RELOC_Z8K_CALLR:	return & r_callr;
    case BFD_RELOC_Z8K_IMM4L:	return & r_imm4l;
    default:			BFD_FAIL ();
      return 0;
    }
}

static reloc_howto_type *
coff_z8k_reloc_name_lookup (bfd *abfd ATTRIBUTE_UNUSED,
			    const char *r_name)
{
  if (strcasecmp (r_imm8.name, r_name) == 0)
    return &r_imm8;
  if (strcasecmp (r_da.name, r_name) == 0)
    return &r_da;
  if (strcasecmp (r_imm32.name, r_name) == 0)
    return &r_imm32;
  if (strcasecmp (r_jr.name, r_name) == 0)
    return &r_jr;
  if (strcasecmp (r_rel16.name, r_name) == 0)
    return &r_rel16;
  if (strcasecmp (r_disp7.name, r_name) == 0)
    return &r_disp7;
  if (strcasecmp (r_callr.name, r_name) == 0)
    return &r_callr;
  if (strcasecmp (r_imm4l.name, r_name) == 0)
    return &r_imm4l;

  return NULL;
}

/* Perform any necessary magic to the addend in a reloc entry.  */

#define CALC_ADDEND(abfd, symbol, ext_reloc, cache_ptr) \
 cache_ptr->addend =  ext_reloc.r_offset;

#define RELOC_PROCESSING(relent,reloc,symbols,abfd,section) \
 reloc_processing(relent, reloc, symbols, abfd, section)

static void
reloc_processing (arelent *relent,
		  struct internal_reloc *reloc,
		  asymbol **symbols,
		  bfd *abfd,
		  asection *section)
{
  relent->address = reloc->r_vaddr;
  rtype2howto (relent, reloc);

  if (reloc->r_symndx == -1 || symbols == NULL)
    relent->sym_ptr_ptr = &bfd_abs_section_ptr->symbol;
  else if (reloc->r_symndx >= 0 && reloc->r_symndx < obj_conv_table_size (abfd))
    relent->sym_ptr_ptr = symbols + obj_convert (abfd)[reloc->r_symndx];
  else
    {
      _bfd_error_handler
	/* xgettext:c-format */
	(_("%pB: warning: illegal symbol index %ld in relocs"),
	 abfd, reloc->r_symndx);
      relent->sym_ptr_ptr = &bfd_abs_section_ptr->symbol;
    }
  relent->addend = reloc->r_offset;
  relent->address -= section->vma;
}

static bool
extra_case (bfd *in_abfd,
	    struct bfd_link_info *link_info,
	    struct bfd_link_order *link_order,
	    arelent *reloc,
	    bfd_byte *data,
	    size_t *src_ptr,
	    size_t *dst_ptr)
{
  asection * input_section = link_order->u.indirect.section;
  bfd_size_type end = bfd_get_section_limit_octets (in_abfd, input_section);
  bfd_size_type reloc_size = bfd_get_reloc_size (reloc->howto);

  if (*src_ptr > end
      || reloc_size > end - *src_ptr)
    {
      link_info->callbacks->einfo
	/* xgettext:c-format */
	(_("%X%P: %pB(%pA): relocation \"%pR\" goes out of range\n"),
	 in_abfd, input_section, reloc);
      return false;
    }


  switch (reloc->howto->type)
    {
    case R_IMM8:
      bfd_put_8 (in_abfd,
		 bfd_coff_reloc16_get_value (reloc, link_info, input_section),
		 data + *dst_ptr);
      *dst_ptr += 1;
      *src_ptr += 1;
      break;

    case R_IMM32:
      /* If no flags are set, assume immediate value.  */
      if (! (*reloc->sym_ptr_ptr)->section->flags)
	{
	  bfd_put_32 (in_abfd,
		      bfd_coff_reloc16_get_value (reloc, link_info,
						  input_section),
		      data + *dst_ptr);
	}
      else
	{
	  bfd_vma dst = bfd_coff_reloc16_get_value (reloc, link_info,
						    input_section);
	  /* Long form: 1SSSSSSS xxxxxxxx AAAAAAA AAAAAAAA  */
	  dst = (dst & 0xffff) | ((dst & 0xff0000) << 8) | 0x80000000;
	  bfd_put_32 (in_abfd, dst, data + *dst_ptr);
	}
      *dst_ptr += 4;
      *src_ptr += 4;
      break;

    case R_IMM32_SHORT:
      {
	/* Short form decided by estimate.  0SSSSSSS OOOOOOOO.
	   Consumes 4 src bytes, writes 2.  */
	bfd_vma dst = bfd_coff_reloc16_get_value (reloc, link_info,
						  input_section);
	bfd_vma seg = (dst >> 16) & 0x7f;
	bfd_vma off = dst & 0xff;
	bfd_put_16 (in_abfd, (seg << 8) | off, data + *dst_ptr);
      }
      *dst_ptr += 2;
      *src_ptr += 4;
      break;

    case R_IMM4L:
      bfd_put_8 (in_abfd,
		 ((bfd_get_8 (in_abfd, data + *src_ptr) & 0xf0)
		  | (0x0f & bfd_coff_reloc16_get_value (reloc, link_info,
							input_section))),
		 data + *dst_ptr);
      *dst_ptr += 1;
      *src_ptr += 1;
      break;

    case R_IMM16:
      bfd_put_16 (in_abfd,
		  bfd_coff_reloc16_get_value (reloc, link_info, input_section),
		  data + *dst_ptr);
      *dst_ptr += 2;
      *src_ptr += 2;
      break;

    case R_JR:
      {
	bfd_vma dst = bfd_coff_reloc16_get_value (reloc, link_info,
						  input_section);
	bfd_vma dot = (*dst_ptr
		       + input_section->output_offset
		       + input_section->output_section->vma);
	/* -1, since we're in the odd byte of the word and the pc has
	   been incremented.  */
	bfd_signed_vma gap = dst - dot - 1;

	if ((gap & 1) != 0 || gap > 254 || gap < -256)
	  {
	    link_info->callbacks->reloc_overflow
	      (link_info, NULL, bfd_asymbol_name (*reloc->sym_ptr_ptr),
	       reloc->howto->name, reloc->addend, input_section->owner,
	       input_section, reloc->address);
	    return false;
	  }

	bfd_put_8 (in_abfd, gap / 2, data + *dst_ptr);
	*dst_ptr += 1;
	*src_ptr += 1;
	break;
      }

    case R_DISP7:
      {
	bfd_vma dst = bfd_coff_reloc16_get_value (reloc, link_info,
						  input_section);
	bfd_vma dot = (*dst_ptr
		       + input_section->output_offset
		       + input_section->output_section->vma);
	bfd_signed_vma gap = dst - dot - 1;

	if ((gap & 1) != 0 || gap > 0 || gap < -254)
	  {
	    link_info->callbacks->reloc_overflow
	      (link_info, NULL, bfd_asymbol_name (*reloc->sym_ptr_ptr),
	       reloc->howto->name, reloc->addend, input_section->owner,
	       input_section, reloc->address);
	    return false;
	  }

	bfd_put_8 (in_abfd,
		   ((bfd_get_8 (in_abfd, data + *src_ptr) & 0x80)
		    + (-gap / 2 & 0x7f)),
		   data + *dst_ptr);
	*dst_ptr += 1;
	*src_ptr += 1;
	break;
      }

    case R_CALLR:
      {
	bfd_vma dst = bfd_coff_reloc16_get_value (reloc, link_info,
						  input_section);
	bfd_vma dot = (*dst_ptr
		       + input_section->output_offset
		       + input_section->output_section->vma);
	bfd_signed_vma gap = dst - dot - 2;

	if ((gap & 1) != 0 || gap > 4096 || gap < -4095)
	  {
	    link_info->callbacks->reloc_overflow
	      (link_info, NULL, bfd_asymbol_name (*reloc->sym_ptr_ptr),
	       reloc->howto->name, reloc->addend, input_section->owner,
	       input_section, reloc->address);
	    return false;
	  }

	bfd_put_16 (in_abfd,
		    ((bfd_get_16 (in_abfd, data + *src_ptr) & 0xf000)
		     | (-gap / 2 & 0x0fff)),
		    data + *dst_ptr);
	*dst_ptr += 2;
	*src_ptr += 2;
	break;
      }

    case R_REL16:
      {
	bfd_vma dst = bfd_coff_reloc16_get_value (reloc, link_info,
						  input_section);
	bfd_vma dot = (*dst_ptr
		       + input_section->output_offset
		       + input_section->output_section->vma);
	bfd_signed_vma gap = dst - dot - 2;

	if (gap > 32767 || gap < -32768)
	  {
	    link_info->callbacks->reloc_overflow
	      (link_info, NULL, bfd_asymbol_name (*reloc->sym_ptr_ptr),
	       reloc->howto->name, reloc->addend, input_section->owner,
	       input_section, reloc->address);
	    return false;
	  }

	bfd_put_16 (in_abfd, gap, data + *dst_ptr);
	*dst_ptr += 2;
	*src_ptr += 2;
	break;
      }

    default:
      link_info->callbacks->einfo
	/* xgettext:c-format */
	(_("%X%P: %pB(%pA): relocation \"%pR\" is not supported\n"),
	 in_abfd, input_section, reloc);
      return false;
    }
  return true;
}

/* Linker relaxation: examine each R_IMM32 relocation to see if the resolved
   segmented address has an offset that fits in 8 bits.  If so, switch to the
   short form (R_IMM32_SHORT), saving 2 bytes per instruction.

   Called from bfd_coff_reloc16_relax_section() in reloc16.c during the
   multi-pass relaxation loop.  The SHRINK parameter is the cumulative
   shrinkage up to this relocation; we return the new cumulative value.  */

/* Compute the cumulative number of bytes deleted before ADDR by
   scanning the reloc vector for R_IMM32_SHORT entries (each saves
   2 bytes vs R_IMM32).  */

static unsigned
z8k_shrinkage_before (bfd_vma addr, arelent **relocs, long reloc_count)
{
  unsigned shrink = 0;
  long i;
  for (i = 0; i < reloc_count && relocs[i]; i++)
    {
      if (relocs[i]->address >= addr)
	break;
      if (relocs[i]->howto->type == R_IMM32_SHORT)
	{
	  shrink += 2;
	}
    }
  return shrink;
}

/* ELF-style relaxation for Z8K segmented short addresses.
   Scans R_IMM32 relocs, converts those whose target offset fits in
   8 bits to R_IMM32_SHORT (saving 2 bytes each).  Multi-pass until
   convergence, then adjusts all symbol values and reloc addresses.  */

static bool
z8k_relax_section (bfd *abfd,
		   asection *input_section,
		   struct bfd_link_info *link_info,
		   bool *again)
{
  long reloc_size, reloc_count;
  arelent **reloc_vector;
  bool changed;
  unsigned total_shrink;

  *again = false;

  if (bfd_link_relocatable (link_info))
    return true;

  reloc_size = bfd_get_reloc_upper_bound (abfd, input_section);
  if (reloc_size <= 0)
    return reloc_size == 0;

  reloc_vector = (arelent **) bfd_malloc (reloc_size);
  if (reloc_vector == NULL)
    return false;

  reloc_count = bfd_canonicalize_reloc (abfd, input_section, reloc_vector,
					_bfd_generic_link_get_symbols (abfd));
  if (reloc_count < 0)
    {
      free (reloc_vector);
      return false;
    }

  /* Multi-pass: keep relaxing until no more changes.  */
  {
  do
    {
      long i;
      changed = false;

      for (i = 0; i < reloc_count && reloc_vector[i]; i++)
	{
	  arelent *rel = reloc_vector[i];

	  if (rel->howto->type != R_IMM32
	      && rel->howto->type != R_IMM32_SHORT)
	    continue;
	  if (! (*rel->sym_ptr_ptr)->section->flags)
	    continue;

	  /* Compute the target's adjusted offset within the segment.
	     The symbol value is still the ORIGINAL (unadjusted) value.
	     Subtract shrinkage from relaxed relocs before the target.  */
	  bfd_vma value = bfd_coff_reloc16_get_value (rel, link_info,
						      input_section);
	  bfd_vma offset = value & 0xffff;
	  asymbol *sym = *rel->sym_ptr_ptr;
	  if (sym->section == input_section)
	    {
	      unsigned s = z8k_shrinkage_before (sym->value, reloc_vector,
						 reloc_count);
	      if (offset >= s)
		offset -= s;
	    }

	  if (offset <= 0xff && rel->howto->type == R_IMM32)
	    {
	      rel->howto = &r_imm32_short;
	      changed = true;
	    }
	  else if (offset > 0xff && rel->howto->type == R_IMM32_SHORT)
	    {
	      rel->howto = &r_imm32;
	      changed = true;
	    }
	}
    }
  while (changed);
  }

  /* Count total shrinkage and adjust symbol values.  */
  total_shrink = z8k_shrinkage_before (input_section->size, reloc_vector,
					reloc_count);

  if (total_shrink > 0)
    {
      /* Adjust all symbol values in this section.  */
      asymbol **syms = _bfd_generic_link_get_symbols (abfd);
      if (syms != NULL)
	{
	  asymbol **sp;
	  for (sp = syms; *sp; sp++)
	    {
	      asymbol *sym = *sp;
	      if (sym->section != input_section)
		continue;

	      unsigned s = z8k_shrinkage_before (sym->value, reloc_vector,
						 reloc_count);
	      if (s > 0)
		{
		  sym->value -= s;
		  if (sym->udata.p != NULL)
		    {
		      struct generic_link_hash_entry *h;
		      h = (struct generic_link_hash_entry *) sym->udata.p;
		      if (h->root.type == bfd_link_hash_defined
			  || h->root.type == bfd_link_hash_defweak)
			h->root.u.def.value = sym->value;
		    }
		}
	    }
	}

      input_section->rawsize = input_section->size;
      input_section->size -= total_shrink;
    }

  free (reloc_vector);
  return true;
}

#define coff_reloc16_extra_cases    extra_case
#define coff_bfd_reloc_type_lookup  coff_z8k_reloc_type_lookup
#define coff_bfd_reloc_name_lookup coff_z8k_reloc_name_lookup

#ifndef bfd_pe_print_pdata
#define bfd_pe_print_pdata	NULL
#endif

#include "coffcode.h"

#undef  coff_bfd_get_relocated_section_contents
#define coff_bfd_get_relocated_section_contents \
  bfd_coff_reloc16_get_relocated_section_contents

#undef  coff_bfd_relax_section
#define coff_bfd_relax_section z8k_relax_section

CREATE_BIG_COFF_TARGET_VEC (z8k_coff_vec, "coff-z8k", 0, 0, '_', NULL, COFF_SWAP_TABLE)
