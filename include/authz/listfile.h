/*
 * QEMU list file authorization driver
 *
 * Copyright (c) 2018 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef QAUTHZ_LIST_FILE_H__
#define QAUTHZ_LIST_FILE_H__

#include "authz/list.h"
#include "qapi/qapi-types-authz.h"
#include "qemu/inotify.h"

#define TYPE_QAUTHZ_LIST_FILE "authz-list-file"

#define QAUTHZ_LIST_FILE_CLASS(klass)                        \
    OBJECT_CLASS_CHECK(QAuthZListFileClass, (klass),        \
                       TYPE_QAUTHZ_LIST_FILE)
#define QAUTHZ_LIST_FILE_GET_CLASS(obj)              \
    OBJECT_GET_CLASS(QAuthZListFileClass, (obj),    \
                      TYPE_QAUTHZ_LIST_FILE)
#define QAUTHZ_LIST_FILE(obj) \
    INTERFACE_CHECK(QAuthZListFile, (obj),          \
                    TYPE_QAUTHZ_LIST_FILE)

typedef struct QAuthZListFile QAuthZListFile;
typedef struct QAuthZListFileClass QAuthZListFileClass;


/**
 * QAuthZListFile:
 *
 * This authorization driver provides a file mechanism
 * for granting access by matching user names against a
 * file of globs. Each match rule has an associated policy
 * and a catch all policy applies if no rule matches
 *
 * To create an instance of this class via QMP:
 *
 *  {
 *    "execute": "object-add",
 *    "arguments": {
 *      "qom-type": "authz-list-file",
 *      "id": "authz0",
 *      "parameters": {
 *        "filename": "/etc/qemu/myvm-vnc.acl",
 *        "refresh": "yes"
 *      }
 *    }
 *  }
 *
 * If 'refresh' is 'yes', inotify is used to monitor for changes
 * to the file and auto-reload the rules.
 *
 * The myvm-vnc.acl file should contain the parameters for
 * the QAuthZList object in JSON format:
 *
 *      {
 *        "rules": [
 *           { "match": "fred", "policy": "allow", "format": "exact" },
 *           { "match": "bob", "policy": "allow", "format": "exact" },
 *           { "match": "danb", "policy": "deny", "format": "exact" },
 *           { "match": "dan*", "policy": "allow", "format": "glob" }
 *        ],
 *        "policy": "deny"
 *      }
 *
 */
struct QAuthZListFile {
    QAuthZ parent_obj;

    QAuthZ *list;
    char *filename;
    bool refresh;
    QInotify *inotify;
};


struct QAuthZListFileClass {
    QAuthZClass parent_class;
};


QAuthZListFile *qauthz_list_file_new(const char *id,
                                     const char *filename,
                                     Error **errp);


#endif /* QAUTHZ_LIST_FILE_H__ */

