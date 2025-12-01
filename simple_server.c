#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>

int eval_expression(const char* expr) {
    const char* p = expr;
    while (isdigit(*p)) {
        p++;
    }
    char op = *p;
    const char* right_str = p + 1;

    int left  = atoi(expr);
    int right = atoi(right_str);

    if (op == '+') {
        result = left + right;
    } else if (op == '-') {
        result = left - right;
    } else if (op == '*') {
        result = left * right;
    } else if (op == '/') {
        result = left / right;
    }
    return result;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);

    // 1. ソケットの作成
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // 再起動時に "Address already in use" を避けるためのオプション
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 2. アドレス構造体の準備
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;          // IPv4
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // どのIP宛でも受ける
    addr.sin_port = htons(port);        // ポート番号（ネットワークバイトオーダ）

    // 3. bind: ソケットにアドレスを紐づける
    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 4. listen: 接続要求を待てる状態にする
    if (listen(server_fd, 5) == -1) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Listening on port %d...\n", port);

    // 5. accept: クライアント1件を受け付ける
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd == -1) {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 6. クライアントからデータを受け取る（ここでは固定長バッファでOK）
    char buffer[1024];
    ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n == -1) {
        perror("recv");
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    buffer[n] = '\0';  // 文字列として使えるようにする
    printf("Received:\n%s\n", buffer);

    char* q = strstr(buffer, "query=");
    if (q == NULL) {
        // 見つからなかったときの処理（とりあえずエラーにしてもOK）
    }

    // ここで q は "query=2+10 HTTP/1.1..." の 'q' の位置
    // 本当に欲しいのは "2+10" の '2' の位置
    char* expr = q + strlen("query=");
    //isdigit
    char expr_buf[64];      // 式だけ入れる用の小さいバッファ
    int i = 0;

    while (expr[i] != '\0' &&
           (isdigit(expr[i]) || expr[i] == '+' || expr[i] == '-' ||
            expr[i] == '*' || expr[i] == '/')) {

        expr_buf[i] = expr[i];
        i++;
    }

    expr_buf[i] = '\0';  // ここで "2+10" だけが入った文字列が完成

    int result = eval_expression(expr_buf);

    char body[32];
    sprintf(body, "%d", result);
    // 例: result が 12 なら body は "12"
    int body_len = strlen(body);  // 2

    char response[256];

    int res_len = snprintf(
        response,
        sizeof(response),
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %d\r\n"
        "\r\n"
        "%s",
        body_len,
        body
    );

    send(client_fd, response, res_len, 0);

    // 7. 後片付け
    close(client_fd);
    close(server_fd);

    return 0;
}
