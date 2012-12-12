/*
 * upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2009 Google Inc.  See LICENSE for details.
 * Author: Josh Haberman <jhaberman@gmail.com>
 *
 * Exposes upb/descriptor to Lua.
 */


int open(lua_State *L) {
}

// Alternate names so that the library can be loaded as upb5_1 etc.
int LUPB_OPENFUNC(upb_descriptor)(lua_State *L) { return open(L); }
