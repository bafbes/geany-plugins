/*
 *  
 *  GeanyGenDoc, a Geany plugin to ease generation of source code documentation
 *  Copyright (C) 2010  Colomban Wendling <ban@herbesfolles.org>
 *  
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 */


#include "ggd-tag-utils.h"

#include <geanyplugin.h>
#include <glib.h>

#include "ggd-utils.h"
#include "ggd-plugin.h" /* to access Geany data/funcs */


/* symbols_get_context_separator() borrowed from Geany since it isn't in the
 * plugin API yet (API v183) */
/* FIXME: replace calls to this function with Geany's
 *        symbols_get_context_separator() when it gets into the plugin API */
static const gchar *
ggd_tag_utils_get_context_separator (filetype_id geany_ft)
{
  switch (geany_ft) {
    case GEANY_FILETYPES_C:     /* for C++ .h headers or C structs */
    case GEANY_FILETYPES_CPP:
    case GEANY_FILETYPES_GLSL:  /* for structs */
      return "::";

    /* avoid confusion with other possible separators in group/section name */
    case GEANY_FILETYPES_CONF:
    case GEANY_FILETYPES_REST:
      return ":::";

    default:
      return ".";
  }
}

/*
 * tag_cmp_by_line:
 * @a: A #TMTag
 * @b: Another #TMTag
 * @data: A pointer that, converted with GPOINTER_TO_INT(), becomes an integer
 *        strictly positive for an ascending sort or strictly negative for a
 *        descending sort. The constants %GGD_SORT_ASC and %GGD_SORT_DESC can be
 *        used here.
 * 
 * Compares two #TMTag<!-- -->s by their lines.
 * 
 * Returns: A positive integer if line(a) > line(b), a negative integer if
 *          line(a) < line(b) and a nul integer (0) if line(a) == line(b).
 */
static gint
tag_cmp_by_line (gconstpointer a,
                 gconstpointer b,
                 gpointer      data)
{
  const TMTag  *t1 = a;
  const TMTag  *t2 = b;
  gint          direction = GPOINTER_TO_INT (data);
  gint          rv;
  
  if (t1->type & tm_tag_file_t || t2->type & tm_tag_file_t) {
    rv = 0;
  } else {
    if (t1->atts.entry.line > t2->atts.entry.line) {
      rv = +direction;
    } else if (t1->atts.entry.line < t2->atts.entry.line) {
      rv = -direction;
    } else {
      rv = 0;
    }
  }
  
  return rv;
}

/* A wrapper for tag_cmp_by_line() that works with g_ptr_array_sort() */
static gint
tag_cmp_by_line_ptr_array (gconstpointer a,
                           gconstpointer b,
                           gpointer      data)
{
  return tag_cmp_by_line (*((const TMTag**)a), *((const TMTag**)b), data);
}

/**
 * ggd_tag_sort_by_line:
 * @tags: A #GPtrArray of #TMTag<!-- -->s
 * @direction: Sort direction: %GGD_SORT_ASC for an ascending sort or
 *             %GGD_SORT_DESC for a descending sort.
 * 
 * Sorts a #GPtrArray of #TMTag<!-- -->s by the tags' line position. The sort
 * direction depend on @direction.
 */
void
ggd_tag_sort_by_line (GPtrArray  *tags,
                      gint        direction)
{
  g_return_if_fail (tags != NULL);
  g_return_if_fail (direction != 0);
  
  g_ptr_array_sort_with_data (tags, tag_cmp_by_line_ptr_array,
                              GINT_TO_POINTER (direction));
}

/**
 * ggd_tag_sort_by_line_to_list:
 * @tags: A #GPtrArray of #TMTag<!-- -->s
 * @direction: Sort direction: %GGD_SORT_ASC for an ascending sort or
 *             %GGD_SORT_DESC for a descending sort.
 * 
 * Creates a sorted list of tags from a #GPtrArray of #TMTag<!-- -->s. The list
 * is sorted by the tags' line position. The sort direction depend on
 * @direction.
 * 
 * <note><para>The tags are not copied; you must then not free the array' items
 * if you still want to use the returned list.</para></note>
 * 
 * Returns: A newly created list of tags that should be freed with
 *          g_list_free().
 */
GList *
ggd_tag_sort_by_line_to_list (const GPtrArray  *tags,
                              gint              direction)
{
  GList  *children = NULL;
  guint   i;
  TMTag  *el;
  
  g_return_val_if_fail (tags != NULL, NULL);
  g_return_val_if_fail (direction != 0, NULL);
  
  GGD_PTR_ARRAY_FOR (tags, i, el) {
    children = g_list_insert_sorted_with_data (children, el,
                                               tag_cmp_by_line,
                                               GINT_TO_POINTER (direction));
  }
  
  return children;
}

/**
 * ggd_tag_find_from_line:
 * @tags: A #GPtrArray of TMTag<!-- -->s
 * @line: Line for which find the tag
 * 
 * Finds the tag that applies for a given line.
 * It actually gets the first tag that precede or is at the exact position of
 * the line.
 * 
 * Returns: A #TMTag, or %NULL if none found.
 */
TMTag *
ggd_tag_find_from_line (const GPtrArray  *tags,
                        gulong            line)
{
  TMTag    *tag = NULL;
  TMTag    *el;
  guint     i;
  
  g_return_val_if_fail (tags != NULL, NULL);
  
  GGD_PTR_ARRAY_FOR (tags, i, el) {
    if (! (el->type & tm_tag_file_t)) {
      if (el->atts.entry.line <= line &&
          (! tag || el->atts.entry.line > tag->atts.entry.line)) {
        tag = el;
      }
    }
  }
  
  return tag;
}

/**
 * ggd_tag_find_at_current_pos:
 * @doc: A #GeanyDocument
 * 
 * Finds the tag that applies for the current position in a #GeanyDocument.
 * See ggd_tag_find_from_line().
 * 
 * Returns: A #TMTag, or %NULL if none found.
 */
TMTag *
ggd_tag_find_at_current_pos (GeanyDocument *doc)
{
  TMTag *tag = NULL;
  
  if (doc && doc->tm_file) {
    tag = ggd_tag_find_from_line (doc->tm_file->tags_array,
                                  sci_get_current_line (doc->editor->sci) + 1);
  }
  
  return tag;
}

/**
 * ggd_tag_find_parent:
 * @tags: A #GPtrArray of #TMTag<!-- -->s containing @tag
 * @geany_ft: The Geany's file type identifier for which tags were generated
 * @child: A #TMTag, child of the tag to find
 * 
 * Finds the parent tag of a #TMTag.
 * 
 * Returns: A #TMTag, or %NULL if @child have no parent.
 */
TMTag *
ggd_tag_find_parent (const GPtrArray *tags,
                     filetype_id      geany_ft,
                     const TMTag     *child)
{
  TMTag *tag = NULL;
  
  g_return_val_if_fail (tags != NULL, NULL);
  g_return_val_if_fail (child != NULL, NULL);
  
  if (! child->atts.entry.scope) {
    /* tag has no parent, we're done */
  } else {
    gchar        *parent_scope = NULL;
    const gchar  *parent_name;
    const gchar  *tmp;
    guint         i;
    TMTag        *el;
    const gchar  *separator;
    gsize         separator_len;
    
    /* scope is of the form a<sep>b<sep>c */
    parent_name = child->atts.entry.scope;
    separator = ggd_tag_utils_get_context_separator (geany_ft);
    separator_len = strlen (separator);
    while ((tmp = strstr (parent_name, separator)) != NULL) {
      parent_name = &tmp[separator_len];
    }
    /* if parent have scope */
    if (parent_name != child->atts.entry.scope) {
      /* the parent scope is the "dirname" of the child's scope */
      parent_scope = g_strndup (child->atts.entry.scope,
                                parent_name - child->atts.entry.scope -
                                  separator_len);
    }
    /*g_debug ("%s: parent_name = %s", G_STRFUNC, parent_name);
    g_debug ("%s: parent_scope = %s", G_STRFUNC, parent_scope);*/
    GGD_PTR_ARRAY_FOR (tags, i, el) {
      if (! (el->type & tm_tag_file_t) &&
          (utils_str_equal (el->name, parent_name) &&
           utils_str_equal (el->atts.entry.scope, parent_scope))) {
        tag = el;
        break;
      }
    }
    g_free (parent_scope);
  }
  
  return tag;
}

static const struct {
  const TMTagType     type;
  const gchar *const  name;
} GGD_tag_types[] = {
  { tm_tag_class_t,           "class"     },
  { tm_tag_enum_t,            "enum"      },
  { tm_tag_enumerator_t,      "enumval"   },
  { tm_tag_field_t,           "field"     },
  { tm_tag_function_t,        "function"  },
  { tm_tag_interface_t,       "interface" },
  { tm_tag_member_t,          "member"    },
  { tm_tag_method_t,          "method"    },
  { tm_tag_namespace_t,       "namespace" },
  { tm_tag_package_t,         "package"   },
  { tm_tag_prototype_t,       "prototype" },
  { tm_tag_struct_t,          "struct"    },
  { tm_tag_typedef_t,         "typedef"   },
  { tm_tag_union_t,           "union"     },
  { tm_tag_variable_t,        "variable"  },
  { tm_tag_externvar_t,       "extern"    },
  { tm_tag_macro_t,           "define"    },
  { tm_tag_macro_with_arg_t,  "macro"     },
  { tm_tag_file_t,            "file"      }
};

/**
 * ggd_tag_type_get_name:
 * @type: A #TMTagType
 * 
 * Tries to convert a #TMTagType to a string.
 * 
 * Returns: A string representing @type that should not be modified or freed,
 *          or %NULL on failure.
 */
const gchar *
ggd_tag_type_get_name (TMTagType type)
{
  guint i;
  
  for (i = 0; i < G_N_ELEMENTS (GGD_tag_types); i++) {
    if (GGD_tag_types[i].type == type) {
      return GGD_tag_types[i].name;
    }
  }
  
  return NULL;
}

/**
 * ggd_tag_type_from_name:
 * @name: A string representing a #TMTag type
 * 
 * Tries to convert string to tag type.
 * 
 * Returns: The #TMTagType that @name stands for, or 0 if @name dosen't stand
 *          for any type.
 */
TMTagType
ggd_tag_type_from_name (const gchar *name)
{
  guint i;
  
  g_return_val_if_fail (name != NULL, 0);
  
  for (i = 0; i < G_N_ELEMENTS (GGD_tag_types); i++) {
    if (utils_str_equal (GGD_tag_types[i].name, name)) {
      return GGD_tag_types[i].type;
    }
  }
  
  return 0;
}

/**
 * ggd_tag_get_type_name:
 * @tag: A #TMTag
 * 
 * Gets the name of a #TMTag's type. See ggd_tag_type_get_name().
 * 
 * Returns: The #TMTag's type name or %NULL on failure.
 */
const gchar *
ggd_tag_get_type_name (const TMTag *tag)
{
  g_return_val_if_fail (tag, NULL);
  
  return ggd_tag_type_get_name (tag->type);
}

/**
 * ggd_tag_resolve_type_hierarchy:
 * @tags: The tag array that contains @tag
 * @geany_ft: The Geany's file type identifier for which tags were generated
 * @tag: A #TMTag to which get the type hierarchy
 * 
 * Gets the type hierarchy of a tag as a string, each element separated by a
 * dot.
 * 
 * Returns: the tag's type hierarchy or %NULL if invalid.
 */
/*
 * FIXME: perhaps we should use array of type's ID rather than a string?
 * FIXME: perhaps drop recursion
 */
gchar *
ggd_tag_resolve_type_hierarchy (const GPtrArray *tags,
                                filetype_id      geany_ft,
                                const TMTag     *tag)
{
  gchar *scope = NULL;
  
  g_return_val_if_fail (tags != NULL, NULL);
  g_return_val_if_fail (tag != NULL, NULL);
  
  if (tag->type & tm_tag_file_t) {
    g_critical (_("Invalid tag"));
  } else {
    TMTag *parent_tag;
    
    parent_tag = ggd_tag_find_parent (tags, geany_ft, tag);
    scope = g_strdup (ggd_tag_get_type_name (tag));
    if (parent_tag) {
      gchar *parent_scope;
      
      parent_scope = ggd_tag_resolve_type_hierarchy (tags, geany_ft, parent_tag);
      if (! parent_scope) {
        /*g_debug ("no parent scope");*/
      } else {
        gchar *tmp;
        
        tmp = g_strconcat (parent_scope, ".", scope, NULL);
        g_free (scope);
        scope = tmp;
        g_free (parent_scope);
      }
    }
  }
  
  return scope;
}

/**
 * ggd_tag_find_from_name:
 * @tags: A #GPtrArray of tags
 * @name: the name of the tag to search for
 * 
 * Gets the tag named @name in @tags
 * 
 * Returns: The #TMTag named @name, or %NULL if none matches
 */
TMTag *
ggd_tag_find_from_name (const GPtrArray *tags,
                        const gchar     *name)
{
  TMTag  *tag = NULL;
  guint   i;
  TMTag  *el;
  
  g_return_val_if_fail (tags != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);
  
  GGD_PTR_ARRAY_FOR (tags, i, el) {
    if (! (el->type & tm_tag_file_t) &&
        utils_str_equal (el->name, name)) {
      tag = el;
      break;
    }
  }
  
  return tag;
}


/*
 * scope_child_matches:
 * @a: parent scope
 * @b: child scope
 * @geany_ft: The Geany's file type identifier for which tags were generated
 * @maxdepth: maximum sub-child level that matches, or < 0 for all to match
 * 
 * Checks if scope @b is inside scope @a. @maxdepth make possible to only match
 * child scope if it have less than @maxdepth parents before scope @a.
 * E.g., with a maximum depth of 1, only direct children will match.
 * 
 * Returns: %TRUE if matches, %FALSE otherwise.
 */
static gboolean
scope_child_matches (const gchar *a,
                     const gchar *b,
                     filetype_id  geany_ft,
                     gint         maxdepth)
{
  gboolean matches = FALSE;
  
  /*g_debug ("trying to match %s against %s", b, a);*/
  if (a && b) {
    for (; *a && *b && *a == *b; a++, b++);
    if (! *a /* we're at the end of the prefix and it matched */) {
      const gchar  *separator;
      
      separator = ggd_tag_utils_get_context_separator (geany_ft);
      if (maxdepth < 0) {
        if (! *b || strncmp (b, separator, strlen (separator)) == 0) {
          matches = TRUE;
        }
      } else {
        while (! matches && maxdepth >= 0) {
          const gchar *tmp;
          
          tmp = strstr (b, separator);
          if (tmp) {
            b = &tmp[2];
            maxdepth --;
          } else {
            if (! *b) {
              matches = TRUE;
            }
            break;
          }
        }
      }
    }
  }
  
  return matches;
}


/**
 * ggd_tag_find_children:
 * @tags: Array of tags that contains @parent
 * @parent: Tag for which get children
 * @geany_ft: The Geany's file type identifier for which tags were generated
 * @depth: Maximum depth for children to be found (< 0 means infinite)
 * @filter: A logical OR of the TMTagType<!-- -->s to match
 * 
 * Finds children tags of a #TMTag that matches @matches.
 * <note><para>The returned list of children is sorted in the order they appears
 * in the source file (by their lines positions)</para></note>
 * 
 * Returns: The list of children found for @parent
 */
GList *
ggd_tag_find_children_filtered (const GPtrArray *tags,
                                const TMTag     *parent,
                                filetype_id      geany_ft,
                                gint             depth,
                                TMTagType        filter)
{
  GList  *children = NULL;
  guint   i;
  TMTag  *el;
  gchar  *fake_scope;
  
  g_return_val_if_fail (tags != NULL, NULL);
  g_return_val_if_fail (parent != NULL, NULL);
  
  if (parent->atts.entry.scope) {
    fake_scope = g_strconcat (parent->atts.entry.scope,
                              ggd_tag_utils_get_context_separator (geany_ft),
                              parent->name, NULL);
  } else {
    fake_scope = g_strdup (parent->name);
  }
  GGD_PTR_ARRAY_FOR (tags, i, el) {
    if (scope_child_matches (fake_scope, el->atts.entry.scope,
                             geany_ft, depth) &&
        el->type & filter) {
      children = g_list_insert_sorted_with_data (children, el,
                                                 tag_cmp_by_line,
                                                 GINT_TO_POINTER (GGD_SORT_ASC));
    }
  }
  g_free (fake_scope);
  
  return children;
}

/**
 * ggd_tag_find_children:
 * @tags: Array of tags that contains @parent
 * @parent: Tag for which get children
 * @geany_ft: The Geany's file type identifier for which tags were generated
 * @depth: Maximum depth for children to be found (< 0 means infinite)
 * 
 * Finds children tags of a #TMTag.
 * <note><para>The returned list of children is sorted in the order they appears
 * in the source file (by their lines positions)</para></note>
 * 
 * Returns: The list of children found for @parent
 */
GList *
ggd_tag_find_children (const GPtrArray *tags,
                       const TMTag     *parent,
                       filetype_id      geany_ft,
                       gint             depth)
{
  return ggd_tag_find_children_filtered (tags, parent, geany_ft,
                                         depth, tm_tag_max_t);
}
