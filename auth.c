#define _CRT_SECURE_NO_WARNINGS
#include "auth.h"

UserList g_users = {0};
User *g_current_user = NULL;

//身份比较函数
void auth_init_users(void) {
    memset(&g_users, 0, sizeof(g_users));
    
    strcpy(g_users.users[0].username, "admin");
    strcpy(g_users.users[0].password, "123456");
    g_users.users[0].role = ROLE_ADMIN;
    
    strcpy(g_users.users[1].username, "guest");
    strcpy(g_users.users[1].password, "guest");
    g_users.users[1].role = ROLE_GUEST;
    
    strcpy(g_users.users[2].username, "tech");
    strcpy(g_users.users[2].password, "tech123");
    g_users.users[2].role = ROLE_TECHNICIAN;
    
    g_users.count = 3;
}

//用户登录
User* auth_login(const char *username, const char *password) {
    for (int i = 0; i < g_users.count; i++) {
        if (strcmp(g_users.users[i].username, username) == 0 &&
            strcmp(g_users.users[i].password, password) == 0) {
            g_current_user = &g_users.users[i];
            return g_current_user;
        }
    }
    return NULL;
}

const char* role_to_string(UserRole role) {
    switch (role) {
        case ROLE_ADMIN:
            return "管理员";
        case ROLE_GUEST:
            return "访客";
        case ROLE_TECHNICIAN:
            return "技术人员";
        default:
            return "未知";
    }
}

//不同角色功能
int auth_has_permission(UserRole role, const char *permission) {
    if (role == ROLE_ADMIN) {
        return 1;
    }
    
    if (role == ROLE_GUEST) {
        if (strcmp(permission, "view_overview") == 0 ||
            strcmp(permission, "view_statistics") == 0) {
            return 1;
        }
        return 0;
    }
    
    if (role == ROLE_TECHNICIAN) {
        if (strcmp(permission, "view_overview") == 0 ||
            strcmp(permission, "view_statistics") == 0 ||
            strcmp(permission, "view_query") == 0 ||
            strcmp(permission, "view_predict") == 0) {
            return 1;
        }
        return 0;
    }
    
    return 0;
}