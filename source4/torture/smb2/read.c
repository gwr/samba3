/* 
   Unix SMB/CIFS implementation.

   SMB2 read test suite

   Copyright (C) Andrew Tridgell 2008
   
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

#include "includes.h"
#include "libcli/smb2/smb2.h"
#include "libcli/smb2/smb2_calls.h"
#include <tevent.h>

#include "torture/torture.h"
#include "torture/smb2/proto.h"


#define CHECK_STATUS(_status, _expected) \
	torture_assert_ntstatus_equal_goto(torture, _status, _expected, \
		 ret, done, "Incorrect status")

#define CHECK_VALUE(v, correct) \
	torture_assert_int_equal_goto(torture, v, correct, \
		 ret, done, "Incorrect value")

#define FNAME "smb2_readtest.dat"
#define DNAME "smb2_readtest.dir"

static bool test_read_eof(struct torture_context *torture, struct smb2_tree *tree)
{
	bool ret = true;
	NTSTATUS status;
	struct smb2_handle h;
	uint8_t buf[64*1024];
	struct smb2_read rd;
	TALLOC_CTX *tmp_ctx = talloc_new(tree);

	ZERO_STRUCT(buf);

	smb2_util_unlink(tree, FNAME);

	status = torture_smb2_testfile(tree, FNAME, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(rd);
	rd.in.file.handle = h;
	rd.in.length      = 5;
	rd.in.offset      = 0;
	status = smb2_read(tree, tree, &rd);
	CHECK_STATUS(status, NT_STATUS_END_OF_FILE);

	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(rd);
	rd.in.file.handle = h;
	rd.in.length = 10;
	rd.in.offset = 0;
	rd.in.min_count = 1;

	status = smb2_read(tree, tmp_ctx, &rd);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(rd.out.data.length, 10);

	rd.in.min_count = 0;
	rd.in.length = 10;
	rd.in.offset = sizeof(buf);
	status = smb2_read(tree, tmp_ctx, &rd);
	CHECK_STATUS(status, NT_STATUS_END_OF_FILE);

	rd.in.min_count = 0;
	rd.in.length = 0;
	rd.in.offset = sizeof(buf);
	status = smb2_read(tree, tmp_ctx, &rd);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(rd.out.data.length, 0);

	rd.in.min_count = 1;
	rd.in.length = 0;
	rd.in.offset = sizeof(buf);
	status = smb2_read(tree, tmp_ctx, &rd);
	CHECK_STATUS(status, NT_STATUS_END_OF_FILE);

	rd.in.min_count = 0;
	rd.in.length = 2;
	rd.in.offset = sizeof(buf) - 1;
	status = smb2_read(tree, tmp_ctx, &rd);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(rd.out.data.length, 1);

	rd.in.min_count = 2;
	rd.in.length = 1;
	rd.in.offset = sizeof(buf) - 1;
	status = smb2_read(tree, tmp_ctx, &rd);
	CHECK_STATUS(status, NT_STATUS_END_OF_FILE);

	rd.in.min_count = 0x10000;
	rd.in.length = 1;
	rd.in.offset = 0;
	status = smb2_read(tree, tmp_ctx, &rd);
	CHECK_STATUS(status, NT_STATUS_END_OF_FILE);

	rd.in.min_count = 0x10000 - 2;
	rd.in.length = 1;
	rd.in.offset = 0;
	status = smb2_read(tree, tmp_ctx, &rd);
	CHECK_STATUS(status, NT_STATUS_END_OF_FILE);

	rd.in.min_count = 10;
	rd.in.length = 5;
	rd.in.offset = 0;
	status = smb2_read(tree, tmp_ctx, &rd);
	CHECK_STATUS(status, NT_STATUS_END_OF_FILE);

done:
	talloc_free(tmp_ctx);
	return ret;
}


static bool test_read_truncate(struct torture_context *torture, struct smb2_tree *tree)
{
	bool ret = true;
	NTSTATUS status;
	struct smb2_handle h;
	uint8_t buf[256*1024];
	struct smb2_create io;
	struct smb2_read rd;
	TALLOC_CTX *tmp_ctx = talloc_new(tree);
	struct smb2_request *req = NULL;
	int rc;

	for (int i = 0 ; i < ARRAY_SIZE(buf) ; i++) {
		buf[i] = 0xAB;
	}

	smb2_util_unlink(tree, FNAME);

	status = torture_smb2_testfile(tree, FNAME, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(rd);
	rd.in.file.handle = h;
	rd.in.length = ARRAY_SIZE(buf) - 500;
	rd.in.offset = 100;
	rd.in.min_count = 1;

	req = smb2_read_send(tree, &rd);
	torture_assert_goto(
		torture,
		req != NULL,
		ret,
		done,
		"smb2_read_send failed\n");

	while (req->state < SMB2_REQUEST_RECV) {
		rc = tevent_loop_once(torture->ev);
		torture_assert_goto(
			torture,
			rc == 0,
			ret,
			done,
			"tevent_loop_once failed\n");
	}

	smb_msleep(50);
	ZERO_STRUCT(io);
	io.in.desired_access     = SEC_FLAG_MAXIMUM_ALLOWED;
	io.in.file_attributes    = FILE_ATTRIBUTE_NORMAL;
	io.in.create_disposition = NTCREATEX_DISP_OVERWRITE_IF;
	io.in.share_access =
		NTCREATEX_SHARE_ACCESS_DELETE|
		NTCREATEX_SHARE_ACCESS_READ|
		NTCREATEX_SHARE_ACCESS_WRITE;
	io.in.create_options = 0;
	io.in.fname = FNAME;

	status = smb2_create(tree, torture, &io);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_read_recv(req, tree, &rd);
	torture_assert_ntstatus_ok_goto(
		torture,
		status,
		ret,
		done,
		"smb2_read_recv failed\n");

done:
	talloc_free(tmp_ctx);
	return ret;
}


static bool test_read_position(struct torture_context *torture, struct smb2_tree *tree)
{
	bool ret = true;
	NTSTATUS status;
	struct smb2_handle h;
	uint8_t buf[64*1024];
	struct smb2_read rd;
	TALLOC_CTX *tmp_ctx = talloc_new(tree);
	union smb_fileinfo info;

	ZERO_STRUCT(buf);

	status = torture_smb2_testfile(tree, FNAME, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(rd);
	rd.in.file.handle = h;
	rd.in.length = 10;
	rd.in.offset = 0;
	rd.in.min_count = 1;

	status = smb2_read(tree, tmp_ctx, &rd);
	CHECK_STATUS(status, NT_STATUS_OK);
	CHECK_VALUE(rd.out.data.length, 10);

	info.generic.level = RAW_FILEINFO_SMB2_ALL_INFORMATION;
	info.generic.in.file.handle = h;

	status = smb2_getinfo_file(tree, tmp_ctx, &info);
	CHECK_STATUS(status, NT_STATUS_OK);
	if (torture_setting_bool(torture, "windows", false)) {
		CHECK_VALUE(info.all_info2.out.position, 0);
	} else {
		CHECK_VALUE(info.all_info2.out.position, 10);
	}

	
done:
	talloc_free(tmp_ctx);
	return ret;
}

static bool test_read_dir(struct torture_context *torture, struct smb2_tree *tree)
{
	bool ret = true;
	NTSTATUS status;
	struct smb2_handle h;
	struct smb2_read rd;
	TALLOC_CTX *tmp_ctx = talloc_new(tree);

	status = torture_smb2_testdir(tree, DNAME, &h);
	if (!NT_STATUS_IS_OK(status)) {
		printf(__location__ " Unable to create test directory '%s' - %s\n", DNAME, nt_errstr(status));
		return false;
	}

	ZERO_STRUCT(rd);
	rd.in.file.handle = h;
	rd.in.length = 10;
	rd.in.offset = 0;
	rd.in.min_count = 1;

	status = smb2_read(tree, tmp_ctx, &rd);
	CHECK_STATUS(status, NT_STATUS_INVALID_DEVICE_REQUEST);
	
	rd.in.min_count = 11;
	status = smb2_read(tree, tmp_ctx, &rd);
	CHECK_STATUS(status, NT_STATUS_INVALID_DEVICE_REQUEST);

	rd.in.length = 0;
	rd.in.min_count = 2592;
	status = smb2_read(tree, tmp_ctx, &rd);
	if (torture_setting_bool(torture, "windows", false)) {
		CHECK_STATUS(status, NT_STATUS_END_OF_FILE);
	} else {
		CHECK_STATUS(status, NT_STATUS_INVALID_DEVICE_REQUEST);
	}

	rd.in.length = 0;
	rd.in.min_count = 0;
	rd.in.channel = 0;
	status = smb2_read(tree, tmp_ctx, &rd);
	if (torture_setting_bool(torture, "windows", false)) {
		CHECK_STATUS(status, NT_STATUS_OK);
	} else {
		CHECK_STATUS(status, NT_STATUS_INVALID_DEVICE_REQUEST);
	}
	
done:
	talloc_free(tmp_ctx);
	return ret;
}

static bool test_read_access(struct torture_context *torture,
			     struct smb2_tree *tree)
{
	bool ret = true;
	NTSTATUS status;
	struct smb2_handle h;
	uint8_t buf[64 * 1024];
	struct smb2_read rd;
	TALLOC_CTX *tmp_ctx = talloc_new(tree);

	ZERO_STRUCT(buf);

	/* create a file */
	smb2_util_unlink(tree, FNAME);

	status = torture_smb2_testfile(tree, FNAME, &h);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_close(tree, h);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* open w/ READ access - success */
	status = torture_smb2_testfile_access(
	    tree, FNAME, &h, SEC_FILE_READ_ATTRIBUTE | SEC_FILE_READ_DATA);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(rd);
	rd.in.file.handle = h;
	rd.in.length = 5;
	rd.in.offset = 0;
	status = smb2_read(tree, tree, &rd);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_close(tree, h);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* open w/ EXECUTE access - success */
	status = torture_smb2_testfile_access(
	    tree, FNAME, &h, SEC_FILE_READ_ATTRIBUTE | SEC_FILE_EXECUTE);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(rd);
	rd.in.file.handle = h;
	rd.in.length = 5;
	rd.in.offset = 0;
	status = smb2_read(tree, tree, &rd);
	CHECK_STATUS(status, NT_STATUS_OK);

	status = smb2_util_close(tree, h);
	CHECK_STATUS(status, NT_STATUS_OK);

	/* open without READ or EXECUTE access - access denied */
	status = torture_smb2_testfile_access(tree, FNAME, &h,
					      SEC_FILE_READ_ATTRIBUTE);
	CHECK_STATUS(status, NT_STATUS_OK);

	ZERO_STRUCT(rd);
	rd.in.file.handle = h;
	rd.in.length = 5;
	rd.in.offset = 0;
	status = smb2_read(tree, tree, &rd);
	CHECK_STATUS(status, NT_STATUS_ACCESS_DENIED);

	status = smb2_util_close(tree, h);
	CHECK_STATUS(status, NT_STATUS_OK);

done:
	talloc_free(tmp_ctx);
	return ret;
}

/* 
   basic testing of SMB2 read
*/
struct torture_suite *torture_smb2_read_init(TALLOC_CTX *ctx)
{
	struct torture_suite *suite = torture_suite_create(ctx, "read");

	torture_suite_add_1smb2_test(suite, "eof", test_read_eof);
	torture_suite_add_1smb2_test(suite, "truncate", test_read_truncate);
	torture_suite_add_1smb2_test(suite, "position", test_read_position);
	torture_suite_add_1smb2_test(suite, "dir", test_read_dir);
	torture_suite_add_1smb2_test(suite, "access", test_read_access);

	suite->description = talloc_strdup(suite, "SMB2-READ tests");

	return suite;
}

static bool test_aio_cancel(struct torture_context *tctx,
			    struct smb2_tree *tree)
{
	struct smb2_handle h;
	uint8_t buf[64 * 1024];
	struct smb2_read r;
	struct smb2_request *req = NULL;
	int rc;
	NTSTATUS status;
	bool ret = true;

	ZERO_STRUCT(buf);

	smb2_util_unlink(tree, FNAME);

	status = torture_smb2_testfile(tree, FNAME, &h);
	torture_assert_ntstatus_ok_goto(
		tctx,
		status,
		ret,
		done,
		"torture_smb2_testfile failed\n");

	status = smb2_util_write(tree, h, buf, 0, ARRAY_SIZE(buf));
	torture_assert_ntstatus_ok_goto(
		tctx,
		status,
		ret,
		done,
		"smb2_util_write failed\n");

	status = smb2_util_close(tree, h);
	torture_assert_ntstatus_ok_goto(
		tctx,
		status,
		ret,
		done,
		"smb2_util_close failed\n");

	status = torture_smb2_testfile_access(
		tree, FNAME, &h, SEC_RIGHTS_FILE_ALL);
	torture_assert_ntstatus_ok_goto(
		tctx,
		status,
		ret,
		done,
		"torture_smb2_testfile_access failed\n");

	r = (struct smb2_read) {
		.in.file.handle = h,
		.in.length      = 1,
		.in.offset      = 0,
		.in.min_count   = 1,
	};

	req = smb2_read_send(tree, &r);
	torture_assert_goto(
		tctx,
		req != NULL,
		ret,
		done,
		"smb2_read_send failed\n");

	while (!req->cancel.can_cancel) {
		rc = tevent_loop_once(tctx->ev);
		torture_assert_goto(
			tctx,
			rc == 0,
			ret,
			done,
			"tevent_loop_once failed\n");
	}

	status = smb2_cancel(req);
	torture_assert_ntstatus_ok_goto(
		tctx,
		status,
		ret,
		done,
		"smb2_cancel failed\n");

	status = smb2_read_recv(req, tree, &r);
	torture_assert_ntstatus_ok_goto(
		tctx,
		status,
		ret,
		done,
		"smb2_read_recv failed\n");

	status = smb2_util_close(tree, h);
	torture_assert_ntstatus_ok_goto(
		tctx,
		status,
		ret,
		done,
		"smb2_util_close failed\n");

done:
	smb2_util_unlink(tree, FNAME);
	return ret;
}

/*
 * aio testing against share with VFS module "delay_inject"
 */
struct torture_suite *torture_smb2_aio_delay_init(TALLOC_CTX *ctx)
{
	struct torture_suite *suite = torture_suite_create(ctx, "aio_delay");

	torture_suite_add_1smb2_test(suite, "aio_cancel", test_aio_cancel);

	suite->description = talloc_strdup(suite, "SMB2 delayed aio tests");

	return suite;
}
