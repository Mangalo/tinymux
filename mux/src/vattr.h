// vattr.h -- Definitions for user-defined attributes.
//
// $Id: vattr.h,v 1.3 2002-07-23 05:36:13 jake Exp $
//

extern ATTR *vattr_rename_LEN(char *, int, char *, int);
extern ATTR *vattr_find_LEN(const char *, int);
extern ATTR *vattr_alloc_LEN(char *, int, int);
extern ATTR *vattr_define_LEN(char *, int, int, int);
extern void  vattr_delete_LEN(char *, int);
extern ATTR *vattr_first(void);
extern ATTR *vattr_next(ATTR *);
extern void  list_vhashstats(dbref);
