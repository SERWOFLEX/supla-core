/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include <alexa/alexatoken.h>
#include <stdlib.h>
#include <string.h>
#include "database.h"
#include "lck.h"
#include "log.h"
#include "user.h"

supla_alexa_token::supla_alexa_token(supla_user *user) {
  lck1 = lck_init();
  lck2 = lck_init();

  this->user = user;
  this->token = NULL;
  this->refresh_token = NULL;
  this->expires_at.tv_sec = 0;
  this->expires_at.tv_usec = 0;
  this->set_at.tv_sec = 0;
  this->set_at.tv_usec = 0;

  set(NULL, NULL, 0, NULL, NULL);

  load();
}

void supla_alexa_token::release_strings(void) {
  if (this->token) {
    free(this->token);
    this->token = NULL;
  }

  if (this->refresh_token) {
    free(this->refresh_token);
    this->refresh_token = NULL;
  }

  if (this->region) {
    free(this->region);
    this->region = NULL;
  }

  if (this->endpoint_scope) {
    free(this->endpoint_scope);
    this->endpoint_scope = NULL;
  }
}

supla_alexa_token::~supla_alexa_token() {
  if (lck1) {
    lck_free(lck1);
    lck1 = NULL;
  }

  if (lck2) {
    lck_free(lck2);
    lck2 = NULL;
  }

  release_strings();
}

int supla_alexa_token::getUserID() { return user->getUserID(); }

void supla_alexa_token::refresh_lock(void) { lck_lock(lck2); }

void supla_alexa_token::refresh_unlock(void) { lck_unlock(lck2); }

void supla_alexa_token::set(const char *token, const char *refresh_token,
                            int expires_in, const char *region,
                            const char *endpoint_scope) {
  lck_lock(lck1);

  release_strings();

  int token_len = token ? strnlen(token, TOKEN_MAXSIZE) : 0;
  int refresh_token_len =
      refresh_token ? strnlen(refresh_token, TOKEN_MAXSIZE) : 0;
  int region_len = region ? strnlen(region, REGION_MAXSIZE) : 0;
  int endpoint_scope_len =
      endpoint_scope ? strnlen(endpoint_scope, ENDPOINTSCOPE_MAXSIZE) : 0;

  if (token_len > 0) {
    this->token = strndup(token, token_len);
  }

  if (refresh_token_len > 0) {
    this->refresh_token = strndup(refresh_token, refresh_token_len);
  }

  if (region_len > 0) {
    this->region = strndup(region, region_len);
  }

  if (endpoint_scope_len > 0) {
    this->endpoint_scope = strndup(endpoint_scope, endpoint_scope_len);
  }

  gettimeofday(&set_at, NULL);

  if (expires_in == 0) {
    expires_in = 3600 * 24 * 365 * 10;
  }

  this->expires_at.tv_sec = set_at.tv_sec + expires_in;
  this->expires_at.tv_usec = set_at.tv_usec;

  lck_unlock(lck1);
}

void supla_alexa_token::load() {
  database *db = new database();

  if (!db->connect() || !db->alexa_load_token(this)) {
    set(NULL, NULL, 0, NULL, NULL);
  }

  delete db;
}

void supla_alexa_token::remove() {
  set(NULL, NULL, 0, NULL, NULL);
  database *db = new database();

  if (db->connect()) {
    db->alexa_remove_token(this);
  }

  delete db;
}

void supla_alexa_token::update(const char *token, const char *refresh_token,
                               int expires_in) {
  char *region = getRegion();
  char *endpoint_scope = getRegion();

  set(token, refresh_token, expires_in, region, endpoint_scope);

  if (region) {
    free(region);
  }

  if (endpoint_scope) {
    free(endpoint_scope);
  }
  database *db = new database();

  if (db->connect()) {
    db->alexa_update_token(this, token, refresh_token, expires_in);
  }

  delete db;
}

bool supla_alexa_token::isTokenExists(void) {
  bool result = false;

  lck_lock(lck1);
  result = token != NULL;
  lck_unlock(lck1);

  return result;
}

bool supla_alexa_token::isRefreshTokenExists(void) {
  bool result = false;

  lck_lock(lck1);
  result = refresh_token != NULL;
  lck_unlock(lck1);

  return result;
}

int supla_alexa_token::expiresIn(void) {
  int result = 0;

  lck_lock(lck1);

  struct timeval now;
  gettimeofday(&now, NULL);
  result = expires_at.tv_sec - now.tv_sec;

  lck_unlock(lck1);

  return result;
}

char *supla_alexa_token::getToken(void) {
  char *result = NULL;

  lck_lock(lck1);

  if (token != NULL) {
    result = strndup(token, TOKEN_MAXSIZE);
  }

  lck_unlock(lck1);

  return result;
}

char *supla_alexa_token::getRefreshToken(void) {
  char *result = NULL;

  lck_lock(lck1);

  if (refresh_token != NULL) {
    result = strndup(refresh_token, TOKEN_MAXSIZE);
  }

  lck_unlock(lck1);

  return result;
}

struct timeval supla_alexa_token::getSetTime(void) {
  struct timeval result;
  result.tv_sec = 0;
  result.tv_usec = 0;

  lck_lock(lck1);
  result = set_at;
  lck_unlock(lck1);

  return result;
}

char *supla_alexa_token::getRegion(void) {
  char *result = NULL;

  lck_lock(lck1);

  if (region != NULL) {
    result = strndup(region, TOKEN_MAXSIZE);
  }

  lck_unlock(lck1);

  return result;
}

char *supla_alexa_token::getEndpointScope(void) {
  char *result = NULL;

  lck_lock(lck1);

  if (endpoint_scope != NULL) {
    result = strndup(endpoint_scope, ENDPOINTSCOPE_MAXSIZE);
  }

  lck_unlock(lck1);

  return result;
}
