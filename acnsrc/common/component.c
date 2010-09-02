/************************************************************************/
/*
Copyright (c) 2010, Engineering Arts (UK)

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

 * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
 * Neither the name of Engineering Arts nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  $Id$

#tabs=3s
*/
/************************************************************************/
#include "component.h"
#include "acnlog.h"

struct component_s *allcomps = NULL;

#if (CONFIG_COMPONENTMEM == MEM_STATIC)
static component_t      components_m[CONFIG_MAX_COMPONENTS];
static struct component_s *freecomps = NULL;
#endif


/************************************************************************/
/*
   Initialize
*/
/************************************************************************/
#if (CONFIG_COMPONENTMEM == MEM_STATIC)
int comp_start(void)
{
   static bool initialized_state = 0;
   struct component_s *comp, *nxcomp;

   if (initialized_state) {
      acnlog(LOG_DEBUG | LOG_COMPM, "compm_init: already initialized");
      return 0;
   }
   // link them all into a list
   nxcomp = NULL;
   for (comp = components_m; comp < components_m + CONFIG_MAX_COMPONENTS; ++comp) {
      comp->next = nxcomp;
      nxcomp = comp;
   }
   freecomps = nxcomp;
   initialized_state = 1;
   return 0;
}
#endif

/************************************************************************/
/*
   Get a new  component
*/
/************************************************************************/

struct component_s *compm_new(void)
{
   struct component_s *comp;
   
#if (CONFIG_COMPONENTMEM == MEM_STATIC)
   if ((comp = freecomps) == NULL) return comp;
   freecomps = comp->next;                      //unlink from free list
   memset(comp, 0, sizeof(struct component_s));
#elif (CONFIG_COMPONENTMEM == MEM_MALLOC)
   if ((comp = calloc(1, sizeof(struct component_s))) == NULL) return comp;
#endif
   comp->usecnt = 1;
   comp->next = allcomps;                       //link to used list
   allcomps = comp;
   return comp;
}

/************************************************************************/
/*
   Find component by it's CID in a list
*/
/************************************************************************/
struct component_s *comp_listfind_by_cid(const cid_t cid, struct component_s *list)
{
   for (;list != NULL; list = list->next) {
      if (cidIsEqual(cid, list->cid)) break;
   }
   return list;
}

/************************************************************************/
/*
   Find component by it's CID in the allcomps list
*/
/************************************************************************/
struct component_s *comp_find_by_cid(const cid_t cid)
{
   return comp_listfind_by_cid(cid, allcomps);
}

/************************************************************************/
/*
   Get a component record for a CID - creating a new one if necessary
*/
/************************************************************************/
struct component_s *comp_get_by_cid(const cid_t cid)
{
   struct component_s *comp;

   if ((comp = comp_find_by_cid(cid)) == NULL) {
      if ((comp = compm_new()) == NULL) {
         errno = ENOMEM;
      } else {
         cidCopy(comp->cid, cid);
      }
   }
   return comp;
}

/************************************************************************/
/*
   Release a component
*/
/************************************************************************/
void comp_release(struct component_s *comp)
{
   struct component_s *cur;

   if (--comp->usecnt == 0) {
      /* unlink from used list */
      if (allcomps == comp)
         allcomps = comp->next;
      else {
         for (cur = allcomps; cur->next != comp; cur = cur->next)
            assert(cur->next != NULL);
         cur->next = comp->next;
      }
#if (CONFIG_COMPONENTMEM == MEM_STATIC)
      comp->next = freecomps;
      freecomps = comp;      //link to free list
#elif (CONFIG_COMPONENTMEM == MEM_MALLOC)
      free(comp);
#endif
   }
}
