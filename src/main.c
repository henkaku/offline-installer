#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <psp2/ctrl.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/net/http.h>
#include <psp2/io/stat.h>
#include <psp2/sysmodule.h>
#include <psp2/net/net.h>
#include <psp2/net/netctl.h>

#include "sqlite3.h"
#include "graphics.h"

#define printf psvDebugScreenPrintf

#define DEBUG 1

#if DEBUG
#define LOG psvDebugScreenPrintf
#else
#define LOG(...)
#endif

#define MAIL_DB "ux0:email/message/mail.db"
#define EXPLOIT_HTML "ux0:email/message/00/00/exploit.html"
#define HENKAKU_BIN "ux0:picture/henkaku.bin"
#define BASE_ADDR "http://go.henkaku.xyz/"

const char *select_max_aid_sql = "SELECT MAX(AccountID) FROM mt_account";
const char *select_max_fid_sql = "SELECT MAX(FolderUID) FROM dt_folder";
const char *select_max_mid_sql = "SELECT MAX(MessageID) FROM dt_message_list";
const char *select_max_muid_sql = "SELECT MAX(MessageUID) FROM dt_message_list";
const char *select_max_setting_sql = "SELECT MAX(SettingID) FROM dt_setting";

const char *mt_account_sql = "INSERT INTO mt_account (AccountID, Enable, Type, AccountName, Name, Address, HostName, Port, EncryptedUserID, EncryptedPassword, UseSSL, AuthType, ViewOrder, UnreadCount, PrimarySmtpID, LeaveMessageFlag, IMAPPathPrefix, UpdateDate) VALUES (%d, 1, 0, 'HENkaku Offline', 'henkaku', 'henkaku@henkaku.xyz', '127.0.0.1', 123, X'00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000', X'00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000', 0, 0, 0, 0, 0, 0, NULL, NULL)";
const char *dt_folder_sql = "INSERT INTO dt_folder (AccountID, FolderID, FolderUID, FolderName, ParentFolderID, FolderPath, UnreadCount, UnreadCountDisplayFlag, Flag, MaxMessageUID, UpdateDate) VALUES(%d, -1, %d, 'EXPLOIT', 0, 'EXPLOIT', 0, 1, 0, NULL, NULL)";
const char *dt_message_list_sql = "INSERT INTO dt_message_list (AccountID, FolderID, MessageID, MessageUID, MessageIDHeader, 'From', OriginalFrom, 'To', OriginalTo, Cc, OriginalCc, Bcc, OriginalBcc, ReplyTo, OriginalReplyTo, InReplyTo, 'References', Subject, OriginalSubject, SentDate, ReceiveDate, Priority, PreviewBody, AttachmentCount, Flag, DownloadedFlag, MessageSize, StatusFlag, ReplyMessageID, ForwardMessageID, Boundary) VALUES(%d, -1, %d, %d, '<henkaku@henkaku.xyz>', 'HENkaku <henkaku@henkaku.xyz>', 'HENkaku <henkaku@henkaku.xyz>', 'HENkaku <henkaku@henkaku.xyz>', 'HENkaku <henkaku@henkaku.xyz>', NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 'HENkaku Offline', 'HENkaku Offline', '2016-07-29 09:00:00', '2016-07-29 09:00:00', 3, '', 0, 1, 1, 0, 0, 0, 0, NULL)";
const char *dt_message_part_sql = "INSERT INTO dt_message_part (AccountID, FolderID, MessageID, PartIndex, Type, OriginalHeader, MimeType, Charset, Encoding, FileName, CID, Section, FilePath, DownloadedFlag, StatusFlag) VALUES (%d, -1, %d, %d, %d, NULL, '%s', 'utf-8', '7BIT', NULL, NULL, %d, '%s', %d, 0)";
const char *dt_setting_sql = "INSERT INTO dt_setting (SettingID, AutoFetch, Signature, BccMe, DefaultAccountID, NewMessageTemplate, ReplyMessageTemplate, ForwardMessageTemplate, BgReceivedFlag, UpdateDate) VALUES (1, NULL, NULL, NULL, -1, NULL, NULL, NULL, 0, NULL)";

static unsigned buttons[] = {
	SCE_CTRL_SELECT,
	SCE_CTRL_START,
	SCE_CTRL_UP,
	SCE_CTRL_RIGHT,
	SCE_CTRL_DOWN,
	SCE_CTRL_LEFT,
	SCE_CTRL_LTRIGGER,
	SCE_CTRL_RTRIGGER,
	SCE_CTRL_TRIANGLE,
	SCE_CTRL_CIRCLE,
	SCE_CTRL_CROSS,
	SCE_CTRL_SQUARE,
};

static const char* tables[] = {
	"dt_folder",
	"dt_message_list",
	"dt_message_part",
	"dt_received_uid_list",
	"dt_task",
	"mt_account",
	NULL
};

int get_key(void) {
	static unsigned prev = 0;
	SceCtrlData pad;
	while (1) {
		memset(&pad, 0, sizeof(pad));
		sceCtrlPeekBufferPositive(0, &pad, 1);
		unsigned new = prev ^ (pad.buttons & prev);
		prev = pad.buttons;
		for (int i = 0; i < sizeof(buttons)/sizeof(*buttons); ++i)
			if (new & buttons[i])
				return buttons[i];

		sceKernelDelayThread(1000); // 1ms
	}
}

void press_exit(void) {
	printf("Press any key to exit this application.\n");
	get_key();
	exit(0);
}

void delete_file(const char *path) {
	printf("Deleting file %s... ", path);
	int ret = sceIoRemove(path);
	if (ret < 0)
		printf("doesn't exist?");
	else
		printf("ok");
	printf("\n");
}

void uninstall_exploit() {
	int ret = 0;
	printf("Uninstalling the exploit\n");

	sqlite3 *db;
	ret = sqlite3_open(MAIL_DB, &db);
	if (ret) {
		printf("Failed to open the mail.db database: %s\n", sqlite3_errmsg(db));
		goto fail;
	}

	sqlite3_stmt *stmt;
	ret = sqlite3_prepare_v2(db, "SELECT AccountID FROM mt_account WHERE AccountName=\"HENkaku Offline\"", -1, &stmt, 0);
	if (ret != SQLITE_OK) {
		printf("Failed to execute select accountid stmt: %s\n", sqlite3_errmsg(db));
		goto fail;
	}

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		int aid = sqlite3_column_int(stmt, 0);
		printf("Deleting email account #%d...\n");
		for (const char **table = &tables[0]; *table; ++table) {
			char sql[256] = {0};
			char *error = NULL;
			printf("Deleting from %s...\n", *table);
			snprintf(sql, sizeof(sql), "DELETE FROM %s WHERE AccountID = %d", *table, aid);
			ret = sqlite3_exec(db, sql, NULL, NULL, &error);
			if (error) {
				printf("Error deleting from table %s: %s\n", *table, error);
				sqlite3_free(error);
				goto fail;
			}
		}
	} else {
		printf("Looks like the exploit is not installed yet.\n");
	}
	sqlite3_finalize(stmt);

	// delete exploit.html and henkaku.bin
	delete_file(EXPLOIT_HTML);
	delete_file(HENKAKU_BIN);

	sqlite3_close(db);
	return;

fail:
	sqlite3_close(db);
	press_exit();
}

int sql_get_max(sqlite3 *db, const char *sql) {
	int id = 1;
	int ret = 0;
	sqlite3_stmt *stmt;
	ret = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
	if (ret != SQLITE_OK) {
		printf("Failed to execute %s, error %s\n", sql, sqlite3_errmsg(db));
		goto fail;
	}
	if (sqlite3_step(stmt) == SQLITE_ROW)
		id = sqlite3_column_int(stmt, 0) + 1;
	sqlite3_finalize(stmt);

	return id;
fail:
	sqlite3_close(db);
	press_exit();
	return 0;
}

void sql_simple_exec(sqlite3 *db, const char *sql) {
	char *error = NULL;
	int ret = 0;
	ret = sqlite3_exec(db, sql, NULL, NULL, &error);
	if (error) {
		printf("Failed to execute %s: %s\n", sql, error);
		sqlite3_free(error);
		goto fail;
	}
	return;
fail:
	sqlite3_close(db);
	press_exit();
}

enum {
	LINE_SIZE = 960,
	PROGRESS_BAR_WIDTH = 400,
	PROGRESS_BAR_HEIGHT = 10,
};

int fg_color;
void draw_rect(int x, int y, int width, int height) {
	void *base = psvDebugScreenGetVram();
	for (int j = y; j < y + height; ++j)
		for (int i = x; i < x + width; ++i)
			((unsigned*)base)[j * LINE_SIZE + i] = fg_color;
}

int download_file(const char *src, const char *dst) {
	int ret;
	printf("downloading %s to %s\n", src, dst);
	int tpl = sceHttpCreateTemplate("henkaku offline", 2, 1);
	if (tpl < 0) {
		printf("sceHttpCreateTemplate: 0x%x\n", tpl);
		return tpl;
	}
	int conn = sceHttpCreateConnectionWithURL(tpl, src, 0);
	if (conn < 0) {
		printf("sceHttpCreateConnectionWithURL: 0x%x\n", conn);
		return conn;
	}
	int req = sceHttpCreateRequestWithURL(conn, 0, src, 0);
	if (req < 0) {
		printf("sceHttpCreateRequestWithURL: 0x%x\n", req);
		return req;
	}
	ret = sceHttpSendRequest(req, NULL, 0);
	if (ret < 0) {
		printf("sceHttpSendRequest: 0x%x\n", ret);
		return ret;
	}
	unsigned char buf[4096] = {0};

	long long length = 0;
	ret = sceHttpGetResponseContentLength(req, &length);

	int fd = sceIoOpen(dst, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 6);
	int total_read = 0;
	if (fd < 0) {
		printf("sceIoOpen: 0x%x\n", fd);
		return fd;
	}
	// draw progress bar background
	fg_color = 0xFF666666;
	draw_rect(psvDebugScreenGetX(), psvDebugScreenGetY(), PROGRESS_BAR_WIDTH, PROGRESS_BAR_HEIGHT);
	fg_color = 0xFFFFFFFF;
	while (1) {
		int read = sceHttpReadData(req, buf, sizeof(buf));
		if (read < 0) {
			printf("sceHttpReadData error! 0x%x\n", read);
			return read;
		}
		if (read == 0)
			break;
		ret = sceIoWrite(fd, buf, read);
		if (ret < 0 || ret != read) {
			printf("sceIoWrite error! 0x%x\n", ret);
			if (ret < 0)
				return ret;
			return -1;
		}
		total_read += read;
		draw_rect(psvDebugScreenGetX() + 1, psvDebugScreenGetY() + 1, (PROGRESS_BAR_WIDTH - 2) * total_read / length, PROGRESS_BAR_HEIGHT - 2);
	}
	printf("\n\n");
	ret = sceIoClose(fd);
	if (ret < 0)
		printf("sceIoClose: 0x%x\n", ret);

	return 0;
}

static void mkdirs(const char *dir) {
	char dir_copy[0x400] = {0};
	snprintf(dir_copy, sizeof(dir_copy) - 2, "%s", dir);
	dir_copy[strlen(dir_copy)] = '/';
	char *c;
	for (c = dir_copy; *c; ++c) {
		if (*c == '/') {
			*c = '\0';
			sceIoMkdir(dir_copy, 0777);
			*c = '/';
		}
	}
}

void install_exploit() {
	printf("Uninstalling the old version first =>\n");
	uninstall_exploit();
	printf("=> all good.\n\n");
	int ret;
	char sql[0x1000];
	char *error = NULL;

	printf("Installing the exploit\n");

	sqlite3 *db;
	ret = sqlite3_open(MAIL_DB, &db);
	if (ret) {
		printf("Failed to open the mail.db database: %s\n", sqlite3_errmsg(db));
		goto fail;
	}

	// make account: mt_account
	int aid = sql_get_max(db, select_max_aid_sql);
	printf("Creating account ID %d\n", aid);
	snprintf(sql, sizeof(sql), mt_account_sql, aid);
	sql_simple_exec(db, sql);

	// make folder: dt_folder
	int fid = sql_get_max(db, select_max_fid_sql);
	printf("Creating exploit folder %d\n", fid);
	snprintf(sql, sizeof(sql), dt_folder_sql, aid, fid);
	sql_simple_exec(db, sql);

	// make message: dt_message_list
	int mid = sql_get_max(db, select_max_mid_sql);
	int muid = sql_get_max(db, select_max_muid_sql);
	printf("Creating message %d:%d\n", mid, muid);
	snprintf(sql, sizeof(sql), dt_message_list_sql, aid, mid, muid);
	sql_simple_exec(db, sql);

	// make message: dt_message_part
	printf("Creating message part2\n");
	snprintf(sql, sizeof(sql), dt_message_part_sql, aid, mid, 1, 0, "TEXT/PLAIN", 1, "ux0:email/message/00/00/exploit.txt", 0);
	sql_simple_exec(db, sql);
	snprintf(sql, sizeof(sql), dt_message_part_sql, aid, mid, 2, 1, "TEXT/HTML", 2, EXPLOIT_HTML, 1);
	sql_simple_exec(db, sql);

	// install dummy settings: dt_setting
	int sid = sql_get_max(db, select_max_setting_sql);
	if (sid == 1) {
		printf("Installing email settings\n");
		sql_simple_exec(db, dt_setting_sql);
	}

	sqlite3_close(db);
	db = NULL;

	mkdirs("ux0:/email/message/00/00/");
	mkdirs("ux0:/picture");

	if (download_file(BASE_ADDR "offline/henkaku.bin", HENKAKU_BIN) < 0)
		goto fail;
	if (download_file(BASE_ADDR "offline/exploit.html", EXPLOIT_HTML) < 0)
		goto fail;

	psvDebugScreenSetFgColor(COLOR_GREEN);
	printf("HENkaku Offline was installed successfully!\n");
	psvDebugScreenSetFgColor(COLOR_WHITE);
	printf("We suggest that you test it by rebooting PS Vita and opening the exploit through the Email app.\n");

	return;

fail:
	if (db)
		sqlite3_close(db);
	press_exit();
}

void netInit() {
	sceSysmoduleLoadModule(SCE_SYSMODULE_NET);
	
	SceNetInitParam netInitParam;
	int size = 1*1024*1024;
	netInitParam.memory = malloc(size);
	netInitParam.size = size;
	netInitParam.flags = 0;
	sceNetInit(&netInitParam);

	sceNetCtlInit();
}

void httpInit() {
	sceSysmoduleLoadModule(SCE_SYSMODULE_HTTP);

	sceHttpInit(1*1024*1024);
}

int main(void) {
	int key = 0;

	netInit();
	httpInit();

	psvDebugScreenInit();

	psvDebugScreenSetFgColor(COLOR_CYAN);	
	printf("offlineInstaller\n\n");
	psvDebugScreenSetFgColor(COLOR_WHITE);

	printf("This application will install offline version of the HENkaku exploit into the Email app.\n");
	printf("Back up all your email data (ux0:/email/ directory) if you have anything important there.\n");
	printf("Make sure the Email app is closed, otherwise the installation may fail.\n");
	printf("\n\n");

	int fd = sceIoOpen("ux0:email/message/mail.db", SCE_O_RDONLY, 0);
	if (fd < 0) {
		printf("It seems that the mail database does not exist (open ux0:email/message/mail.db err=0x%x)\n", fd);
		printf("You should open the Email app from LiveArea and then close it without adding an account.\n");
		press_exit();
	}
	sceIoClose(fd);

again:
	printf("Press X to install the exploit.\n");
	printf("Press O to uninstall the exploit.\n");
	printf("\n");

	key = get_key();
	switch (key) {
	case SCE_CTRL_CROSS:
		install_exploit();
		break;
	case SCE_CTRL_CIRCLE:
		uninstall_exploit();
		break;
	default:
		printf("Invalid input, try again.\n\n");
		goto again;
	}

	press_exit();
}
