/*
   Unix SMB/CIFS implementation.

   local testing of the nss wrapper

   Copyright (C) Guenther Deschner 2009-2010

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <unistd.h>

#include "includes.h"

#include "torture/torture.h"
#include "torture/local/proto.h"
#include "lib/replace/system/passwd.h"

static bool copy_passwd(struct torture_context *tctx,
			const struct passwd *pwd,
			struct passwd *p)
{
	p->pw_name	= talloc_strdup(tctx, pwd->pw_name);
	torture_assert(tctx, (p->pw_name != NULL || pwd->pw_name == NULL), __location__);
	p->pw_passwd	= talloc_strdup(tctx, pwd->pw_passwd);
	torture_assert(tctx, (p->pw_passwd != NULL || pwd->pw_passwd == NULL), __location__);
	p->pw_uid	= pwd->pw_uid;
	p->pw_gid	= pwd->pw_gid;
	p->pw_gecos	= talloc_strdup(tctx, pwd->pw_gecos);
	torture_assert(tctx, (p->pw_gecos != NULL || pwd->pw_gecos == NULL), __location__);
	p->pw_dir	= talloc_strdup(tctx, pwd->pw_dir);
	torture_assert(tctx, (p->pw_dir != NULL || pwd->pw_dir == NULL), __location__);
	p->pw_shell	= talloc_strdup(tctx, pwd->pw_shell);
	torture_assert(tctx, (p->pw_shell != NULL || pwd->pw_shell == NULL), __location__);

	return true;
}

static void print_passwd(struct passwd *pwd)
{
	printf("%s:%s:%lu:%lu:%s:%s:%s\n",
	       pwd->pw_name,
	       pwd->pw_passwd,
	       (unsigned long)pwd->pw_uid,
	       (unsigned long)pwd->pw_gid,
	       pwd->pw_gecos,
	       pwd->pw_dir,
	       pwd->pw_shell);
}


static bool test_getpwnam(struct torture_context *tctx,
			  const char *name,
			  struct passwd *pwd_p)
{
	struct passwd *pwd;
	int ret;

	torture_comment(tctx, "Testing getpwnam: %s\n", name);

	errno = 0;
	pwd = getpwnam(name);
	ret = errno;
	torture_assert(tctx, (pwd != NULL), talloc_asprintf(tctx,
		       "getpwnam(%s) failed - %d - %s",
		       name, ret, strerror(ret)));

	if (pwd_p != NULL) {
		torture_assert(tctx, copy_passwd(tctx, pwd, pwd_p), __location__);
	}

	return true;
}

static bool test_getpwnam_r(struct torture_context *tctx,
			    const char *name,
			    struct passwd *pwd_p)
{
	struct passwd pwd, *pwdp;
	char buffer[4096];
	int ret;

	torture_comment(tctx, "Testing getpwnam_r: %s\n", name);

	ret = getpwnam_r(name, &pwd, buffer, sizeof(buffer), &pwdp);
	torture_assert(tctx, ret == 0, talloc_asprintf(tctx,
		       "getpwnam_r(%s) failed - %d - %s",
		       name, ret, strerror(ret)));

	print_passwd(&pwd);

	if (pwd_p != NULL) {
		torture_assert(tctx, copy_passwd(tctx, &pwd, pwd_p), __location__);
	}

	return true;
}

static bool test_getpwuid(struct torture_context *tctx,
			  uid_t uid,
			  struct passwd *pwd_p)
{
	struct passwd *pwd;
	int ret;

	torture_comment(tctx, "Testing getpwuid: %lu\n", (unsigned long)uid);

	errno = 0;
	pwd = getpwuid(uid);
	ret = errno;
	torture_assert(tctx, (pwd != NULL), talloc_asprintf(tctx,
		       "getpwuid(%lu) failed - %d - %s",
		       (unsigned long)uid, ret, strerror(ret)));

	print_passwd(pwd);

	if (pwd_p != NULL) {
		torture_assert(tctx, copy_passwd(tctx, pwd, pwd_p), __location__);
	}

	return true;
}

static bool test_getpwuid_r(struct torture_context *tctx,
			    uid_t uid,
			    struct passwd *pwd_p)
{
	struct passwd pwd, *pwdp;
	char buffer[4096];
	int ret;

	torture_comment(tctx, "Testing getpwuid_r: %lu\n", (unsigned long)uid);

	ret = getpwuid_r(uid, &pwd, buffer, sizeof(buffer), &pwdp);
	torture_assert(tctx, ret == 0, talloc_asprintf(tctx,
		       "getpwuid_r(%lu) failed - %d - %s",
		       (unsigned long)uid, ret, strerror(ret)));

	print_passwd(&pwd);

	if (pwd_p != NULL) {
		torture_assert(tctx, copy_passwd(tctx, &pwd, pwd_p), __location__);
	}

	return true;
}


static bool copy_group(struct torture_context *tctx,
		       const struct group *grp,
		       struct group *g)
{
	int i;

	g->gr_name	= talloc_strdup(tctx, grp->gr_name);
	torture_assert(tctx, (g->gr_name != NULL || grp->gr_name == NULL), __location__);
	g->gr_passwd	= talloc_strdup(tctx, grp->gr_passwd);
	torture_assert(tctx, (g->gr_passwd != NULL || grp->gr_passwd == NULL), __location__);
	g->gr_gid	= grp->gr_gid;
	g->gr_mem	= NULL;

	for (i=0; grp->gr_mem && grp->gr_mem[i]; i++) {
		g->gr_mem = talloc_realloc(tctx, g->gr_mem, char *, i + 2);
		torture_assert(tctx, (g->gr_mem != NULL), __location__);
		g->gr_mem[i] = talloc_strdup(g->gr_mem, grp->gr_mem[i]);
		torture_assert(tctx, (g->gr_mem[i] != NULL), __location__);
		g->gr_mem[i+1] = NULL;
	}

	return true;
}

static void print_group(struct group *grp)
{
	int i;
	printf("%s:%s:%lu:",
	       grp->gr_name,
	       grp->gr_passwd,
	       (unsigned long)grp->gr_gid);

	if ((grp->gr_mem == NULL) || !grp->gr_mem[0]) {
		printf("\n");
		return;
	}

	for (i=0; grp->gr_mem[i+1]; i++) {
		printf("%s,", grp->gr_mem[i]);
	}
	printf("%s\n", grp->gr_mem[i]);
}

static bool test_getgrnam(struct torture_context *tctx,
			  const char *name,
			  struct group *grp_p)
{
	struct group *grp;
	int ret;

	torture_comment(tctx, "Testing getgrnam: %s\n", name);

	errno = 0;
	grp = getgrnam(name);
	ret = errno;
	torture_assert(tctx, (grp != NULL), talloc_asprintf(tctx,
		       "getgrnam(%s) failed - %d - %s",
		       name, ret, strerror(ret)));

	print_group(grp);

	if (grp_p != NULL) {
		torture_assert(tctx, copy_group(tctx, grp, grp_p), __location__);
	}

	return true;
}

static bool test_getgrnam_r(struct torture_context *tctx,
			    const char *name,
			    struct group *grp_p)
{
	struct group grp, *grpp;
	char buffer[4096];
	int ret;

	torture_comment(tctx, "Testing getgrnam_r: %s\n", name);

	ret = getgrnam_r(name, &grp, buffer, sizeof(buffer), &grpp);
	torture_assert(tctx, ret == 0, talloc_asprintf(tctx,
		       "getgrnam_r(%s) failed - %d - %s",
		       name, ret, strerror(ret)));

	print_group(&grp);

	if (grp_p != NULL) {
		torture_assert(tctx, copy_group(tctx, &grp, grp_p), __location__);
	}

	return true;
}


static bool test_getgrgid(struct torture_context *tctx,
			  gid_t gid,
			  struct group *grp_p)
{
	struct group *grp;
	int ret;

	torture_comment(tctx, "Testing getgrgid: %lu\n", (unsigned long)gid);

	errno = 0;
	grp = getgrgid(gid);
	ret = errno;
	torture_assert(tctx, (grp != NULL), talloc_asprintf(tctx,
		       "getgrgid(%lu) failed - %d - %s",
		       (unsigned long)gid, ret, strerror(ret)));

	print_group(grp);

	if (grp_p != NULL) {
		torture_assert(tctx, copy_group(tctx, grp, grp_p), __location__);
	}

	return true;
}

static bool test_getgrgid_r(struct torture_context *tctx,
			    gid_t gid,
			    struct group *grp_p)
{
	struct group grp, *grpp;
	char buffer[4096];
	int ret;

	torture_comment(tctx, "Testing getgrgid_r: %lu\n", (unsigned long)gid);

	ret = getgrgid_r(gid, &grp, buffer, sizeof(buffer), &grpp);
	torture_assert(tctx, ret == 0, talloc_asprintf(tctx,
		       "getgrgid_r(%lu) failed - %d - %s",
		       (unsigned long)gid, ret, strerror(ret)));

	print_group(&grp);

	if (grp_p != NULL) {
		torture_assert(tctx, copy_group(tctx, &grp, grp_p), __location__);
	}

	return true;
}

static bool test_enum_passwd(struct torture_context *tctx,
			     struct passwd **pwd_array_p,
			     size_t *num_pwd_p)
{
	struct passwd *pwd;
	struct passwd *pwd_array = NULL;
	size_t num_pwd = 0;

	torture_comment(tctx, "Testing setpwent\n");
	setpwent();

	while ((pwd = getpwent()) != NULL) {
		torture_comment(tctx, "Testing getpwent\n");

		print_passwd(pwd);
		if (pwd_array_p && num_pwd_p) {
			pwd_array = talloc_realloc(tctx, pwd_array, struct passwd, num_pwd+1);
			torture_assert(tctx, pwd_array, "out of memory");
			copy_passwd(tctx, pwd, &pwd_array[num_pwd]);
			num_pwd++;
		}
	}

	torture_comment(tctx, "Testing endpwent\n");
	endpwent();

	if (pwd_array_p) {
		*pwd_array_p = pwd_array;
	}
	if (num_pwd_p) {
		*num_pwd_p = num_pwd;
	}

	return true;
}

static bool test_enum_r_passwd(struct torture_context *tctx,
			       struct passwd **pwd_array_p,
			       size_t *num_pwd_p)
{
	struct passwd pwd, *pwdp;
	struct passwd *pwd_array = NULL;
	size_t num_pwd = 0;
	char buffer[4096];
	int ret;

	torture_comment(tctx, "Testing setpwent\n");
	setpwent();

#ifdef HAVE_GETPWENT_R /* getpwent_r not supported on macOS */
	while (1) {
		torture_comment(tctx, "Testing getpwent_r\n");

#ifdef SOLARIS_GETPWENT_R
		pwdp = getpwent_r(&pwd, buffer, sizeof(buffer));
		ret = (pwdp == NULL) ? ENOENT : 0;
#else /* SOLARIS_GETPWENT_R */
		ret = getpwent_r(&pwd, buffer, sizeof(buffer), &pwdp);
#endif /* SOLARIS_GETPWENT_R */
		if (ret != 0) {
			if (ret != ENOENT) {
				torture_comment(tctx, "got %d return code\n", ret);
			}
			break;
		}
		print_passwd(&pwd);
		if (pwd_array_p && num_pwd_p) {
			pwd_array = talloc_realloc(tctx, pwd_array, struct passwd, num_pwd+1);
			torture_assert(tctx, pwd_array, "out of memory");
			copy_passwd(tctx, &pwd, &pwd_array[num_pwd]);
			num_pwd++;
		}
	}
#endif /* getpwent_r not supported on macOS */

	torture_comment(tctx, "Testing endpwent\n");
	endpwent();

	if (pwd_array_p) {
		*pwd_array_p = pwd_array;
	}
	if (num_pwd_p) {
		*num_pwd_p = num_pwd;
	}

	return true;
}

static bool torture_assert_passwd_equal(struct torture_context *tctx,
					const struct passwd *p1,
					const struct passwd *p2,
					const char *comment)
{
	torture_assert_str_equal(tctx, p1->pw_name, p2->pw_name, comment);
	torture_assert_str_equal(tctx, p1->pw_passwd, p2->pw_passwd, comment);
	torture_assert_int_equal(tctx, p1->pw_uid, p2->pw_uid, comment);
	torture_assert_int_equal(tctx, p1->pw_gid, p2->pw_gid, comment);
	torture_assert_str_equal(tctx, p1->pw_gecos, p2->pw_gecos, comment);
	torture_assert_str_equal(tctx, p1->pw_dir, p2->pw_dir, comment);
	torture_assert_str_equal(tctx, p1->pw_shell, p2->pw_shell, comment);

	return true;
}

static bool test_passwd(struct torture_context *tctx)
{
	int i;
	struct passwd *pwd, pwd1, pwd2;
	size_t num_pwd;

	torture_assert(tctx, test_enum_passwd(tctx, &pwd, &num_pwd),
					      "failed to enumerate passwd");

	for (i=0; i < num_pwd; i++) {
		torture_assert(tctx, test_getpwnam(tctx, pwd[i].pw_name, &pwd1),
			"failed to call getpwnam for enumerated user");
		torture_assert(tctx, torture_assert_passwd_equal(tctx, &pwd[i], &pwd1,
			"getpwent and getpwnam gave different results"),
			__location__);
		torture_assert(tctx, test_getpwuid(tctx, pwd[i].pw_uid, &pwd2),
			"failed to call getpwuid for enumerated user");
		torture_assert(tctx, torture_assert_passwd_equal(tctx, &pwd[i], &pwd2,
			"getpwent and getpwuid gave different results"),
			__location__);
		torture_assert(tctx, torture_assert_passwd_equal(tctx, &pwd1, &pwd2,
			"getpwnam and getpwuid gave different results"),
			__location__);
	}

	return true;
}

static bool test_passwd_r(struct torture_context *tctx)
{
	int i;
	struct passwd *pwd, pwd1, pwd2;
	size_t num_pwd;

	torture_assert(tctx, test_enum_r_passwd(tctx, &pwd, &num_pwd),
						"failed to enumerate passwd");

	for (i=0; i < num_pwd; i++) {
		torture_assert(tctx, test_getpwnam_r(tctx, pwd[i].pw_name, &pwd1),
			"failed to call getpwnam_r for enumerated user");
		torture_assert(tctx, torture_assert_passwd_equal(tctx, &pwd[i], &pwd1,
			"getpwent_r and getpwnam_r gave different results"),
			__location__);
		torture_assert(tctx, test_getpwuid_r(tctx, pwd[i].pw_uid, &pwd2),
			"failed to call getpwuid_r for enumerated user");
		torture_assert(tctx, torture_assert_passwd_equal(tctx, &pwd[i], &pwd2,
			"getpwent_r and getpwuid_r gave different results"),
			__location__);
		torture_assert(tctx, torture_assert_passwd_equal(tctx, &pwd1, &pwd2,
			"getpwnam_r and getpwuid_r gave different results"),
			__location__);
	}

	return true;
}

static bool test_passwd_r_cross(struct torture_context *tctx)
{
	int i;
	struct passwd *pwd, pwd1, pwd2, pwd3, pwd4;
	size_t num_pwd;

	torture_assert(tctx, test_enum_r_passwd(tctx, &pwd, &num_pwd),
						"failed to enumerate passwd");

	for (i=0; i < num_pwd; i++) {
		torture_assert(tctx, test_getpwnam_r(tctx, pwd[i].pw_name, &pwd1),
			"failed to call getpwnam_r for enumerated user");
		torture_assert(tctx, torture_assert_passwd_equal(tctx, &pwd[i], &pwd1,
			"getpwent_r and getpwnam_r gave different results"),
			__location__);
		torture_assert(tctx, test_getpwuid_r(tctx, pwd[i].pw_uid, &pwd2),
			"failed to call getpwuid_r for enumerated user");
		torture_assert(tctx, torture_assert_passwd_equal(tctx, &pwd[i], &pwd2,
			"getpwent_r and getpwuid_r gave different results"),
			__location__);
		torture_assert(tctx, torture_assert_passwd_equal(tctx, &pwd1, &pwd2,
			"getpwnam_r and getpwuid_r gave different results"),
			__location__);
		torture_assert(tctx, test_getpwnam(tctx, pwd[i].pw_name, &pwd3),
			"failed to call getpwnam for enumerated user");
		torture_assert(tctx, torture_assert_passwd_equal(tctx, &pwd[i], &pwd3,
			"getpwent_r and getpwnam gave different results"),
			__location__);
		torture_assert(tctx, test_getpwuid(tctx, pwd[i].pw_uid, &pwd4),
			"failed to call getpwuid for enumerated user");
		torture_assert(tctx, torture_assert_passwd_equal(tctx, &pwd[i], &pwd4,
			"getpwent_r and getpwuid gave different results"),
			__location__);
		torture_assert(tctx, torture_assert_passwd_equal(tctx, &pwd3, &pwd4,
			"getpwnam and getpwuid gave different results"),
			__location__);
	}

	return true;
}

static bool test_enum_group(struct torture_context *tctx,
			    struct group **grp_array_p,
			    size_t *num_grp_p)
{
	struct group *grp;
	struct group *grp_array = NULL;
	size_t num_grp = 0;

	torture_comment(tctx, "Testing setgrent\n");
	setgrent();

	while ((grp = getgrent()) != NULL) {
		torture_comment(tctx, "Testing getgrent\n");

		print_group(grp);
		if (grp_array_p && num_grp_p) {
			grp_array = talloc_realloc(tctx, grp_array, struct group, num_grp+1);
			torture_assert(tctx, grp_array, "out of memory");
			copy_group(tctx, grp, &grp_array[num_grp]);
			num_grp++;
		}
	}

	torture_comment(tctx, "Testing endgrent\n");
	endgrent();

	if (grp_array_p) {
		*grp_array_p = grp_array;
	}
	if (num_grp_p) {
		*num_grp_p = num_grp;
	}

	return true;
}

static bool test_enum_r_group(struct torture_context *tctx,
			      struct group **grp_array_p,
			      size_t *num_grp_p)
{
	struct group grp, *grpp;
	struct group *grp_array = NULL;
	size_t num_grp = 0;
	char buffer[4096];
	int ret;

	torture_comment(tctx, "Testing setgrent\n");
	setgrent();

#ifdef HAVE_GETGRENT_R /* getgrent_r not supported on macOS */
	while (1) {
		torture_comment(tctx, "Testing getgrent_r\n");

#ifdef SOLARIS_GETGRENT_R
		grpp = getgrent_r(&grp, buffer, sizeof(buffer));
		ret = (grpp == NULL) ? ENOENT : 0;
#else /* SOLARIS_GETGRENT_R */
		ret = getgrent_r(&grp, buffer, sizeof(buffer), &grpp);
#endif /* SOLARIS_GETGRENT_R */
		if (ret != 0) {
			if (ret != ENOENT) {
				torture_comment(tctx, "got %d return code\n", ret);
			}
			break;
		}
		print_group(&grp);
		if (grp_array_p && num_grp_p) {
			grp_array = talloc_realloc(tctx, grp_array, struct group, num_grp+1);
			torture_assert(tctx, grp_array, "out of memory");
			copy_group(tctx, &grp, &grp_array[num_grp]);
			num_grp++;
		}
	}
#endif /* getgrent_r not supported on macOS */

	torture_comment(tctx, "Testing endgrent\n");
	endgrent();

	if (grp_array_p) {
		*grp_array_p = grp_array;
	}
	if (num_grp_p) {
		*num_grp_p = num_grp;
	}

	return true;
}

static bool torture_assert_group_equal(struct torture_context *tctx,
				       const struct group *g1,
				       const struct group *g2,
				       const char *comment)
{
	int i;
	torture_assert_str_equal(tctx, g1->gr_name, g2->gr_name, comment);
	torture_assert_str_equal(tctx, g1->gr_passwd, g2->gr_passwd, comment);
	torture_assert_int_equal(tctx, g1->gr_gid, g2->gr_gid, comment);
	torture_assert(tctx, !(g1->gr_mem && !g2->gr_mem), __location__);
	torture_assert(tctx, !(!g1->gr_mem && g2->gr_mem), __location__);
	if (!g1->gr_mem && !g2->gr_mem) {
		return true;
	}
	for (i=0; g1->gr_mem[i] && g2->gr_mem[i]; i++) {
		torture_assert_str_equal(tctx, g1->gr_mem[i], g2->gr_mem[i], comment);
	}

	return true;
}

static bool test_group(struct torture_context *tctx)
{
	int i;
	struct group *grp, grp1, grp2;
	size_t num_grp;

	torture_assert(tctx, test_enum_group(tctx, &grp, &num_grp),
					     "failed to enumerate group");

	for (i=0; i < num_grp; i++) {
		torture_assert(tctx, test_getgrnam(tctx, grp[i].gr_name, &grp1),
			"failed to call getgrnam for enumerated user");
		torture_assert(tctx, torture_assert_group_equal(tctx, &grp[i], &grp1,
			"getgrent and getgrnam gave different results"),
			__location__);
		torture_assert(tctx, test_getgrgid(tctx, grp[i].gr_gid, &grp2),
			"failed to call getgrgid for enumerated user");
		torture_assert(tctx, torture_assert_group_equal(tctx, &grp[i], &grp2,
			"getgrent and getgrgid gave different results"),
			__location__);
		torture_assert(tctx, torture_assert_group_equal(tctx, &grp1, &grp2,
			"getgrnam and getgrgid gave different results"),
			__location__);
	}

	return true;
}

static bool test_group_r(struct torture_context *tctx)
{
	int i;
	struct group *grp, grp1, grp2;
	size_t num_grp;

	torture_assert(tctx, test_enum_r_group(tctx, &grp, &num_grp),
					       "failed to enumerate group");

	for (i=0; i < num_grp; i++) {
		torture_assert(tctx, test_getgrnam_r(tctx, grp[i].gr_name, &grp1),
			"failed to call getgrnam_r for enumerated user");
		torture_assert(tctx, torture_assert_group_equal(tctx, &grp[i], &grp1,
			"getgrent_r and getgrnam_r gave different results"),
			__location__);
		torture_assert(tctx, test_getgrgid_r(tctx, grp[i].gr_gid, &grp2),
			"failed to call getgrgid_r for enumerated user");
		torture_assert(tctx, torture_assert_group_equal(tctx, &grp[i], &grp2,
			"getgrent_r and getgrgid_r gave different results"),
			__location__);
		torture_assert(tctx, torture_assert_group_equal(tctx, &grp1, &grp2,
			"getgrnam_r and getgrgid_r gave different results"),
			__location__);
	}

	return true;
}

static bool test_group_r_cross(struct torture_context *tctx)
{
	int i;
	struct group *grp, grp1, grp2, grp3, grp4;
	size_t num_grp;

	torture_assert(tctx, test_enum_r_group(tctx, &grp, &num_grp),
					       "failed to enumerate group");

	for (i=0; i < num_grp; i++) {
		torture_assert(tctx, test_getgrnam_r(tctx, grp[i].gr_name, &grp1),
			"failed to call getgrnam_r for enumerated user");
		torture_assert(tctx, torture_assert_group_equal(tctx, &grp[i], &grp1,
			"getgrent_r and getgrnam_r gave different results"),
			__location__);
		torture_assert(tctx, test_getgrgid_r(tctx, grp[i].gr_gid, &grp2),
			"failed to call getgrgid_r for enumerated user");
		torture_assert(tctx, torture_assert_group_equal(tctx, &grp[i], &grp2,
			"getgrent_r and getgrgid_r gave different results"),
			__location__);
		torture_assert(tctx, torture_assert_group_equal(tctx, &grp1, &grp2,
			"getgrnam_r and getgrgid_r gave different results"),
			__location__);
		torture_assert(tctx, test_getgrnam(tctx, grp[i].gr_name, &grp3),
			"failed to call getgrnam for enumerated user");
		torture_assert(tctx, torture_assert_group_equal(tctx, &grp[i], &grp3,
			"getgrent_r and getgrnam gave different results"),
			__location__);
		torture_assert(tctx, test_getgrgid(tctx, grp[i].gr_gid, &grp4),
			"failed to call getgrgid for enumerated user");
		torture_assert(tctx, torture_assert_group_equal(tctx, &grp[i], &grp4,
			"getgrent_r and getgrgid gave different results"),
			__location__);
		torture_assert(tctx, torture_assert_group_equal(tctx, &grp3, &grp4,
			"getgrnam and getgrgid gave different results"),
			__location__);
	}

	return true;
}

#ifdef HAVE_GETGROUPLIST

#ifdef	SOLARIS_GETGRENT_R	/* better config... */
extern int _getgroupsbymember(const char *, gid_t[], int, int);
#endif

static bool test_getgrouplist(struct torture_context *tctx,
			      const char *user,
			      gid_t gid,
			      gid_t **gids_p,
			      int *num_gids_p)
{
	int ret;
	int num_groups = 0;
	gid_t *groups = NULL;

	torture_comment(tctx, "Testing getgrouplist: %s\n", user);

#ifdef	SOLARIS_GETGRENT_R
	num_groups = sysconf(_SC_NGROUPS_MAX);
	ret = 0;
#else
	ret = getgrouplist(user, gid, NULL, &num_groups);
#endif
	if (ret == -1 || num_groups != 0) {

		groups = talloc_array(tctx, gid_t, num_groups);
		torture_assert(tctx, groups, "out of memory\n");

#ifdef	SOLARIS_GETGRENT_R	/* better config... */
		groups[0] = gid;
		ret = _getgroupsbymember(user, groups, num_groups, 1);
		if (ret > 0)
			num_groups = ret;
		else
			ret = -1;
#else
		ret = getgrouplist(user, gid, groups, &num_groups);
#endif
	}

	torture_assert(tctx, (ret != -1), "failed to call getgrouplist");

	torture_comment(tctx, "%s is member in %d groups\n", user, num_groups);

	if (gids_p) {
		*gids_p = groups;
	}
	if (num_gids_p) {
		*num_gids_p = num_groups;
	}

	return true;
}
#endif /* HAVE_GETGROUPLIST */

static bool test_user_in_group(struct torture_context *tctx,
			       const struct passwd *pwd,
			       const struct group *grp)
{
	int i;

	for (i=0; grp->gr_mem && grp->gr_mem[i] != NULL; i++) {
		if (strequal(grp->gr_mem[i], pwd->pw_name)) {
			return true;
		}
	}

	return false;
}

static bool test_membership_user(struct torture_context *tctx,
				 const struct passwd *pwd,
				 struct group *grp_array,
				 size_t num_grp)
{
	int num_user_groups = 0;
	int num_user_groups_from_enum = 0;
	gid_t *user_groups = NULL;
	int g, i;
	bool primary_group_had_user_member = false;

#ifdef HAVE_GETGROUPLIST
	torture_assert(tctx, test_getgrouplist(tctx,
					       pwd->pw_name,
					       pwd->pw_gid,
					       &user_groups,
					       &num_user_groups),
					       "failed to test getgrouplist");
#endif /* HAVE_GETGROUPLIST */

	for (g=0; g < num_user_groups; g++) {
		torture_assert(tctx, test_getgrgid(tctx, user_groups[g], NULL),
			"failed to find the group the user is a member of");
	}


	for (i=0; i < num_grp; i++) {

		struct group grp = grp_array[i];

		if (test_user_in_group(tctx, pwd, &grp)) {

			struct group current_grp;
			num_user_groups_from_enum++;

			torture_assert(tctx, test_getgrnam(tctx, grp.gr_name, &current_grp),
					"failed to find the group the user is a member of");

			if (current_grp.gr_gid == pwd->pw_gid) {
				torture_comment(tctx, "primary group %s of user %s lists user as member\n",
						current_grp.gr_name,
						pwd->pw_name);
				primary_group_had_user_member = true;
			}

			continue;
		}
	}

	if (!primary_group_had_user_member) {
		num_user_groups_from_enum++;
	}

	torture_assert_int_equal(tctx, num_user_groups, num_user_groups_from_enum,
		"getgrouplist and real inspection of grouplist gave different results\n");

	return true;
}

static bool test_membership(struct torture_context *tctx)
{
	const char *old_pwd = getenv("NSS_WRAPPER_PASSWD");
	const char *old_group = getenv("NSS_WRAPPER_GROUP");
	struct passwd *pwd;
	size_t num_pwd;
	struct group *grp;
	size_t num_grp;
	int i;

	if (!old_pwd || !old_group) {
		torture_comment(tctx, "ENV NSS_WRAPPER_PASSWD or NSS_WRAPPER_GROUP not set\n");
		torture_skip(tctx, "nothing to test\n");
	}

	torture_assert(tctx, test_enum_passwd(tctx, &pwd, &num_pwd),
					      "failed to enumerate passwd");
	torture_assert(tctx, test_enum_group(tctx, &grp, &num_grp),
					     "failed to enumerate group");

	for (i=0; i < num_pwd; i++) {

		torture_assert(tctx, test_membership_user(tctx, &pwd[i], grp, num_grp),
			"failed to test membership for user");

	}

	return true;
}

static bool test_enumeration(struct torture_context *tctx)
{
	const char *old_pwd = getenv("NSS_WRAPPER_PASSWD");
	const char *old_group = getenv("NSS_WRAPPER_GROUP");

	if (!old_pwd || !old_group) {
		torture_comment(tctx, "ENV NSS_WRAPPER_PASSWD or NSS_WRAPPER_GROUP not set\n");
		torture_skip(tctx, "nothing to test\n");
	}

	torture_assert(tctx, test_passwd(tctx),
			"failed to test users");
	torture_assert(tctx, test_group(tctx),
			"failed to test groups");

	return true;
}

static bool test_reentrant_enumeration(struct torture_context *tctx)
{
	const char *old_pwd = getenv("NSS_WRAPPER_PASSWD");
	const char *old_group = getenv("NSS_WRAPPER_GROUP");

	if (!old_pwd || !old_group) {
		torture_comment(tctx, "ENV NSS_WRAPPER_PASSWD or NSS_WRAPPER_GROUP not set\n");
		torture_skip(tctx, "nothing to test\n");
	}

	torture_comment(tctx, "Testing re-entrant calls\n");

	torture_assert(tctx, test_passwd_r(tctx),
			"failed to test users");
	torture_assert(tctx, test_group_r(tctx),
			"failed to test groups");

	return true;
}

static bool test_reentrant_enumeration_crosschecks(struct torture_context *tctx)
{
	const char *old_pwd = getenv("NSS_WRAPPER_PASSWD");
	const char *old_group = getenv("NSS_WRAPPER_GROUP");

	if (!old_pwd || !old_group) {
		torture_comment(tctx, "ENV NSS_WRAPPER_PASSWD or NSS_WRAPPER_GROUP not set\n");
		torture_skip(tctx, "nothing to test\n");
	}

	torture_comment(tctx, "Testing re-entrant calls with cross checks\n");

	torture_assert(tctx, test_passwd_r_cross(tctx),
			"failed to test users");
	torture_assert(tctx, test_group_r_cross(tctx),
			"failed to test groups");

	return true;
}

static bool test_passwd_duplicates(struct torture_context *tctx)
{
	size_t i, d;
	struct passwd *pwd;
	size_t num_pwd;
	int duplicates = 0;

	torture_assert(tctx, test_enum_passwd(tctx, &pwd, &num_pwd),
	    "failed to enumerate passwd");

	for (i=0; i < num_pwd; i++) {
		const char *current_name = pwd[i].pw_name;
		for (d=0; d < num_pwd; d++) {
			const char *dup_name = pwd[d].pw_name;
			if (d == i) {
				continue;
			}
			if (!strequal(current_name, dup_name)) {
				continue;
			}

			torture_warning(tctx, "found duplicate names:");
			print_passwd(&pwd[d]);
			print_passwd(&pwd[i]);
			duplicates++;
		}
	}

	if (duplicates) {
		torture_fail(tctx, talloc_asprintf(tctx, "found %d duplicate names", duplicates));
	}

	return true;
}

static bool test_group_duplicates(struct torture_context *tctx)
{
	size_t i, d;
	struct group *grp;
	size_t num_grp;
	int duplicates = 0;

	torture_assert(tctx, test_enum_group(tctx, &grp, &num_grp),
		"failed to enumerate group");

	for (i=0; i < num_grp; i++) {
		const char *current_name = grp[i].gr_name;
		for (d=0; d < num_grp; d++) {
			const char *dup_name = grp[d].gr_name;
			if (d == i) {
				continue;
			}
			if (!strequal(current_name, dup_name)) {
				continue;
			}

			torture_warning(tctx, "found duplicate names:");
			print_group(&grp[d]);
			print_group(&grp[i]);
			duplicates++;
		}
	}

	if (duplicates) {
		torture_fail(tctx, talloc_asprintf(tctx, "found %d duplicate names", duplicates));
	}

	return true;
}


static bool test_duplicates(struct torture_context *tctx)
{
	const char *old_pwd = getenv("NSS_WRAPPER_PASSWD");
	const char *old_group = getenv("NSS_WRAPPER_GROUP");

	if (!old_pwd || !old_group) {
		torture_comment(tctx, "ENV NSS_WRAPPER_PASSWD or NSS_WRAPPER_GROUP not set\n");
		torture_skip(tctx, "nothing to test\n");
	}

	torture_assert(tctx, test_passwd_duplicates(tctx),
			"failed to test users");
	torture_assert(tctx, test_group_duplicates(tctx),
			"failed to test groups");

	return true;
}


struct torture_suite *torture_local_nss(TALLOC_CTX *mem_ctx)
{
	struct torture_suite *suite = torture_suite_create(mem_ctx, "nss");

	torture_suite_add_simple_test(suite, "enumeration", test_enumeration);
	torture_suite_add_simple_test(suite, "reentrant enumeration", test_reentrant_enumeration);
	torture_suite_add_simple_test(suite, "reentrant enumeration crosschecks", test_reentrant_enumeration_crosschecks);
	torture_suite_add_simple_test(suite, "membership", test_membership);
	torture_suite_add_simple_test(suite, "duplicates", test_duplicates);

	return suite;
}
