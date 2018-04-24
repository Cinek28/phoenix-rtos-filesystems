/*
 * Phoenix-RTOS
 *
 * ext2
 *
 * object.h
 *
 * Copyright 2017 Phoenix Systems
 * Author: Kamil Amanowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _OS_PHOENIX_OBJECT_H_
#define _OS_PHOENIX_OBJECT_H_ /* object.h */


enum {
	ST_ERROR,
	ST_LOCKED,
	ST_UNLOCKED
};


typedef struct _jffs2_object_t {
	oid_t oid;
	struct jffs2_inode_info *inode_info;

	u32 refs;
	u32 type;
	u8  state;

	rbnode_t node;
	handle_t lock;

	int rmaxgap;
	int lmaxgap;

} jffs2_object_t;


void object_init(void);


jffs2_object_t *object_create(int type);


jffs2_object_t *object_get(unsigned int id);


void object_destroy(jffs2_object_t *o);


void object_put(jffs2_object_t *o);


#endif /* _OS_PHOENIX_OBJECT_H_ */
