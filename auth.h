#ifndef AUTH_H
#define AUTH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_USERNAME_LEN 32
#define MAX_PASSWORD_LEN 32
#define MAX_USER_COUNT   10

typedef enum {
    ROLE_ADMIN = 1, //管理员
    ROLE_GUEST = 2, //访客
    ROLE_TECHNICIAN = 3  //技术人员
} UserRole;

typedef struct {
    char username[MAX_USERNAME_LEN]; 
    char password[MAX_PASSWORD_LEN];
    UserRole role; //角色
} User;

typedef struct {
    User users[MAX_USER_COUNT]; //用户数组
    int count; //用户数量
} UserList;

extern UserList g_users;
extern User *g_current_user;

void auth_init_users(void);

//登录用户
User* auth_login(const char *username, const char *password);

const char* role_to_string(UserRole role);

int auth_has_permission(UserRole role, const char *permission);

#endif