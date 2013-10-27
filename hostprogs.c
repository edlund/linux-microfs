/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2012 Erik Edlund <erik.edlund@32767.se>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "hostprogs.h"

int fykshuffle(void** slots, const size_t length)
{
	if (length > RAND_MAX) {
		errno = EINVAL;
		return -1;
	}
	if (length > 0) {
		for (size_t i = length - 1; i > 0; i--) {
			size_t n = rand() % (i + 1);
			void* tmp = slots[i];
			slots[i] = slots[n];
			slots[n] = tmp;
		}
	}
	return 0;
}

int hostprog_werror = 0;
int hostprog_verbosity = 0;

int hostprog_scandirsort(const struct dirent** a, const struct dirent** b)
{
	return strcmp((*a)->d_name, (*b)->d_name);
}

int hostprog_stack_create(struct hostprog_stack** const s,
	const size_t size, const size_t growth)
{
	if (size == 0 || growth < 2) {
		errno = EINVAL;
		return -1;
	}
	if (!*s && !(*s = malloc(sizeof(**s)))) {
		goto err_nomem;
	}
	memset(*s, 0, sizeof(**s));
	(*s)->st_size = size;
	(*s)->st_growth = growth;
	if (!((*s)->st_slots = malloc((*s)->st_size * sizeof(*(*s)->st_slots)))) {
		goto err_slots_nomem;
	}
	return 0;
	
err_slots_nomem:
	hostprog_stack_destroy(*s);
	*s = NULL;
err_nomem:
	errno = ENOMEM;
	return -1;
}

int hostprog_stack_destroy(struct hostprog_stack* const s)
{
	if (!s) {
		errno = EINVAL;
		return -1;
	}
	free(s->st_slots);
	free(s);
	return 0;
}

int hostprog_stack_size(const struct hostprog_stack* const s)
{
	if (!s) {
		errno = EINVAL;
		return -1;
	}
	return s->st_index;
}

int _hostprog_stack_push(struct hostprog_stack* s, void* v)
{
	if (!s) {
		errno = EINVAL;
		return -1;
	}
	if (s->st_index == s->st_size) {
		s->st_size *= s->st_growth;
		s->st_slots = realloc(s->st_slots, s->st_size * sizeof(*s->st_slots));
		if (!s->st_slots) {
			errno = ENOMEM;
			return -1;
		}
	}
	s->st_slots[s->st_index++] = v;
	return 0;
}

int _hostprog_stack_top(struct hostprog_stack* const s, void** v)
{
	if (!s || !v) {
		errno = EINVAL;
		return -1;
	}
	*v = s->st_slots[s->st_index - 1];
	return 0;
}

int _hostprog_stack_pop(struct hostprog_stack* const s, void** v)
{
	if (!s || !v) {
		errno = EINVAL;
		return -1;
	} else if (s->st_index == 0) {
		errno = EFAULT;
		return -1;
	}
	*v = s->st_slots[--s->st_index];
	return 0;
}

int hostprog_path_create(struct hostprog_path** dpath, const char* dname,
	const size_t maxnamelen, const size_t mingrowth)
{
	if (!*dpath && !(*dpath = malloc(sizeof(**dpath)))) {
		goto err_nomem;
	}
	memset(*dpath, 0, sizeof(**dpath));
	(*dpath)->p_maxnamelen = maxnamelen;
	(*dpath)->p_mingrowth = mingrowth;
	(*dpath)->p_pathsz = mingrowth + 1;
	(*dpath)->p_path = malloc((*dpath)->p_pathsz);
	if (!(*dpath)->p_path)
		goto err_path_nomem;
	
	if (hostprog_stack_create(&(*dpath)->p_separators, 64, 64) != 0)
		goto err_stack_nomem;
	
	return dname? hostprog_path_append(*dpath, dname): 0;
	
err_stack_nomem:
err_path_nomem:
	hostprog_path_destroy(*dpath);
	*dpath = NULL;
err_nomem:
	errno = ENOMEM;
	return -1;
}

int hostprog_path_append(struct hostprog_path* const dpath, const char* dpart)
{
	if (!dpath || !dpart || *dpart == '\0')
		goto err_inval;
	
	size_t extralen = 2;
	size_t dpartlen = strlen(dpart);
	if (dpartlen > dpath->p_maxnamelen)
		goto err_inval;
	
	/* It is okay that a name starts with a slash or is the
	 * actual root dir, but if it is the root, then it has to be
	 * the first part added to the path.
	 */
	if (*dpart == '/') {
		if (strcmp(dpart, "/") == 0) {
			if (dpath->p_pathlen > 0)
				goto err_inval;
		} else if (dpart[1] == '/') {
			/* Trim leading slashes, it is possible to end up with the
			 * root dir.
			 */
			dpart++;
			dpartlen--;
			for (const char* c = dpart; *(c + 1) == '/'; c++) {
				dpart++;
				dpartlen--;
			}
			/* Adding the root to the root is not okay. Maybe it should
			 * be?
			 */
			if (strcmp(dpart, "/") == 0 && strcmp(dpath->p_path, "/") == 0)
				goto err_inval;
		}
		extralen--;
	}
	
	/* Always trim trailing slashes.
	 */
	while (dpartlen > 1 && dpart[dpartlen - 1] == '/')
		dpartlen--;
	
	/* Do we have to grow the buffer or will the part fit?
	 */
	if (dpath->p_pathlen + dpartlen + extralen > dpath->p_pathsz) {
		dpath->p_pathsz += (
			(dpartlen + extralen < dpath->p_mingrowth)?
				dpath->p_mingrowth + extralen:
				dpartlen * 2 + extralen
		);
		dpath->p_path = realloc(dpath->p_path, dpath->p_pathsz);
		if (!dpath->p_path)
			goto err_realloc;
	}
	
	/* Insert a slash if necessary, checking for the special case
	 * where only the root dir exists.
	 */
	if (*dpart == '/' && strcmp(dpath->p_path, "/") == 0) {
		dpart++;
		dpartlen--;
	} else if (dpath->p_pathlen && *dpart != '/') {
		if (hostprog_stack_push(dpath->p_separators,
				(hostprog_stack_int_t)dpath->p_pathlen))
			goto err_stack;
		dpath->p_path[dpath->p_pathlen++] = '/';
	}
	
	/* Now add the path part.
	 */
	size_t old_pathlen = dpath->p_pathlen;
	memcpy(dpath->p_path + dpath->p_pathlen, dpart, dpartlen);
	dpath->p_pathlen += dpartlen;
	
	/* Then prune it from any excess slashes.
	 */
	for (size_t i = old_pathlen; i < dpath->p_pathlen - 1; i++) {
		if (dpath->p_path[i] == '/' && dpath->p_path[i + 1] == '/') {
			memmove(dpath->p_path + i, dpath->p_path + i + 1,
				dpath->p_pathlen - i);
			i--;
			dpath->p_pathlen--;
		}
	}
	
	dpath->p_path[dpath->p_pathlen] = '\0';
	
	/* And remember any slashes that have been added so that it
	 * is easy to find parent dirs in %hostprog_path_dirname()
	 * later on.
	 */
	for (size_t i = old_pathlen; i < dpath->p_pathlen - 1; i++) {
		if (i > 0 && dpath->p_path[i] == '/') {
			if (hostprog_stack_push(dpath->p_separators,
					(hostprog_stack_int_t)i))
				goto err_stack;
		}
	}
	
	return 0;
	
err_stack:
err_realloc:
	errno = ENOMEM;
	return -1;
err_inval:
	errno = EINVAL;
	return -1;
}

int hostprog_path_lvls(struct hostprog_path* const dpath)
{
	if (!dpath) {
		errno = EINVAL;
		return -1;
	}
	if (dpath->p_pathlen == 0)
		return 0;
	
	int root_present = *dpath->p_path == '/';
	int separators_present = hostprog_stack_size(dpath->p_separators);
	
	if (root_present && strcmp(dpath->p_path, "/") == 0)
		return 1;
	return root_present + separators_present + 1;
}

/* Please note that %hostprog_path_dirname() does not work
 * exactly as %dirname(3) as it is possible to end up with a
 * completely empty path. It would not be difficult to make
 * it behave as %dirname(3), but is it necessary?
 */
int hostprog_path_dirname(struct hostprog_path* const dpath)
{
	if (!dpath) {
		errno = EINVAL;
		return -1;
	}
	int separators = hostprog_stack_size(dpath->p_separators);
	if (separators == 0 && *dpath->p_path == '/') {
		dpath->p_pathlen = dpath->p_pathlen > 1? 1: 0;
		dpath->p_path[dpath->p_pathlen] = '\0';
	} else if (dpath->p_pathlen > 0) {
		hostprog_stack_int_t dir = 0;
		if (separators)
			hostprog_stack_pop(dpath->p_separators, &dir);
		dpath->p_pathlen -= dpath->p_pathlen - dir;
		dpath->p_path[dpath->p_pathlen] = '\0';
	} else {
		/* Should this be an error?
		 */
	}
	return 0;
}

int hostprog_path_dirnamelvl(struct hostprog_path* const dpath,
	const int lvl)
{
	int lvls = hostprog_path_lvls(dpath);
	
	if (lvl < 0 || lvl > lvls) {
		errno = EINVAL;
		return -1;
	}
	
	if (lvl == 0)
		return hostprog_path_reset(dpath);
	
	while (lvl < lvls--)
		hostprog_path_dirname(dpath);
	return 0;
}

int hostprog_path_reset(struct hostprog_path* const dpath)
{
	if (!dpath) {
		errno = EINVAL;
		return -1;
	}
	dpath->p_pathlen = 0;
	dpath->p_path[dpath->p_pathlen] = '\0';
	return 0;
}

int hostprog_path_destroy(struct hostprog_path* const dpath)
{
	if (!dpath) {
		errno = EINVAL;
		return -1;
	}
	hostprog_stack_destroy(dpath->p_separators);
	free(dpath->p_path);
	free(dpath);
	return 0;
}

int hostprog_path_dotdir(const char* dname)
{
	if (dname && dname[0] == '.') {
		if (dname[1] == '\0')
			return 1;
		if (dname[1] == '.') {
			if (dname[2] == '\0')
				return 1;
		}
	}
	return 0;
}

