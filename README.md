## 如何使用 C 语言编写一个简单的 HTTP 服务器

> 代码位置在本仓库的`simple-http-server`文件夹

你可能会想知道网络服务器是如何“提供”文件的，以及它们如何传递组成网站的文件，以便在你的屏幕上显示。了解网络服务器的实现确实是“通过构建学习”的一种好方法。

尽管用C语言实现一个网络服务器听起来非常基础，并且在底层细节上很复杂，但这可能有助于你更好地理解HTTP是如何工作的，以及服务器是如何与客户端互动的。让我们开始吧！

### Socket 是如何工作的

在构建网络服务器之前，你需要了解“套接字”(Socket)是如何工作的。

如果在主机上运行的程序（或进程）就像一座房子，那么套接字就像一扇门，允许邮件进出。当房子里的人接收或发送信件时，他或她可以不关心外界邮件是如何送达的。

![](image/1.jpeg)

通过使用套接字网络接口，我们的网络服务器可以使用C标准库中的一组函数 `<sys/socket.h>`，使我们的服务器能够通过互联网与客户端“对话”。这些客户端也在使用套接字与我们进行通信，因此套接字基本上就像是一个关于如何在互联网上互相交流的共识。

![](image/2.jpeg)

### 创建Server Socket

为了开始，我首先为服务器创建并配置了一个套接字。我配置了一些参数：

- AF_INET：使用IPv4（与IPv6相比）
- SOCK_STREAM：使用TCP（与UDP相比）
- INADDR_ANY：服务器接受来自任何网络接口的连接

```c
int server_fd;
struct sockaddr_in server_addr;

// create server socket
if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
}

// config socket
server_addr.sin_family = AF_INET;
server_addr.sin_addr.s_addr = INADDR_ANY;
server_addr.sin_port = htons(PORT);

// bind socket to port
if (bind(server_fd,
        (struct sockaddr *)&server_addr,
        sizeof(server_addr)) < 0) {
   perror("bind failed");
   exit(EXIT_FAILURE);
}

// listen for connections
if (listen(server_fd, 10) < 0) {
    perror("listen failed");
    exit(EXIT_FAILURE);
}
```

`bind()` 函数将套接字绑定到一个端口（我在这里使用的是8080），这意味着套接字将监听任何试图连接到8080端口的客户端。`listen()` 函数接受最大待处理连接数（我将其设置为10）。

现在，我的服务器已经设置完毕，准备接受来自客户端的连接。

### 处理客户端连接（Handle Client Connections）

服务器启动并运行后，下一步是处理客户端连接。我使用了一个无限循环来持续监听新的客户端。

当一个客户端连接时，服务器接受该连接并创建一个新线程来处理客户端的HTTP请求。这样，服务器就可以并发处理多个客户端。

```c
while (1) {
    // client info
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    int *client_fd = malloc(sizeof(int));

    // accept client connection
    if ((*client_fd = accept(server_fd,
                            (struct sockaddr *)&client_addr,
                            &client_addr_len)) < 0) {
        perror("accept failed");
        continue;
    }

    // create a new thread to handle client request
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, handle_client, (void *)client_fd);
    pthread_detach(thread_id);
}
```

我定义了一个函数 `handle_client()`，用于处理来自客户端的请求。我只实现了 `GET` 方法，因为它很好地展示了服务器如何“提供”文件给客户端。

`handle_client()` 接收请求数据，提取请求的文件名，并解码 URL（例如，将 `%20` 转换为空格）。然后，它识别文件扩展名，构建一个包含请求文件的HTTP响应，并将其发送回客户端。

```c
void *handle_client(void *arg) {
    int client_fd = *((int *)arg);
    char *buffer = (char *)malloc(BUFFER_SIZE * sizeof(char));

    // receive request data from client and store into buffer
    ssize_t bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
        // check if request is GET
        regex_t regex;
        regcomp(&regex, "^GET /([^ ]*) HTTP/1", REG_EXTENDED);
        regmatch_t matches[2];

        if (regexec(&regex, buffer, 2, matches, 0) == 0) {
            // extract filename from request and decode URL
            buffer[matches[1].rm_eo] = '\0';
            const char *url_encoded_file_name = buffer + matches[1].rm_so;
            char *file_name = url_decode(url_encoded_file_name);

            // get file extension
            char file_ext[32];
            strcpy(file_ext, get_file_extension(file_name));

            // build HTTP response
            char *response = (char *)malloc(BUFFER_SIZE * 2 * sizeof(char));
            size_t response_len;
            build_http_response(file_name, file_ext, response, &response_len);

            // send HTTP response to client
            send(client_fd, response, response_len, 0);

            free(response);
            free(file_name);
        }
        regfree(&regex);
    }
    close(client_fd);
    free(arg);
    free(buffer);
    return NULL;
}
```

### 构建HTTP响应（Build HTTP Response）

该函数首先根据文件扩展名构建一个带有适当 MIME 类型的 HTTP 头（例如，如果请求的是 `.jpg` 文件，MIME 类型为 `image/jpeg`）。如果文件不存在，函数会创建一个 `404 Not Found` 响应。否则，它会获取文件的大小，并将 `200 OK` 头添加到响应缓冲区中。

![](image/3.png)

接下来，它读取文件内容并将其附加到响应缓冲区中。返回给 `handle_client()` 的响应缓冲区会被发送回客户端。我将 `BUFFER_SIZE` 设置为 1MB，这意味着服务器可以处理大小最多为 1MB 的任何 HTTP 响应。

## 使用esp32实现一个简单的HTTP服务器

> 源码位置在：`examples/protocols/http_server/simple`

我们还是来实现一个简单的 GET 请求的 HTTP 服务器

具体代码见本仓库的`esp32-simple-http-server`

## 使用ESP32实现采集麦克风声音并播放的功能

麦克风本质上是一种传感器，将声音这种模拟信号以一定的采样率转换成数字信号。

> 源码位置在 `examples/peripherals/i2s/i2s_codec/i2s_es8311`

I2S（Inter-IC Sound，集成电路内置音频总线）是一种同步串行通信协议，通常用于在两个数字音频设备之间传输音频数据。

ESP32 包含 2 个 I2S 外设。通过配置这些外设，可以借助 I2S 驱动来输入和输出采样数据。

标准或 TDM 通信模式下的 I2S 总线包含以下几条线路：

- MCLK：主时钟线。该信号线可选，具体取决于从机，主要用于向 I2S 从机提供参考时钟。
- BCLK：位时钟线。用于数据线的位时钟。
- WS：字（声道）选择线。通常用于识别声道（除 PDM 模式外）。
- DIN/DOUT：串行数据输入/输出线。如果 DIN 和 DOUT 被配置到相同的 GPIO，数据将在内部回环。

每个 I2S 控制器都具备以下功能，可由 I2S 驱动进行配置：

- 可用作系统主机或从机
- 可用作发射器或接收器
- DMA 控制器支持流数据采样，CPU 无需单独复制每个采样数据

每个控制器都支持 RX 或 TX 单工通信。由于 RX 与 TX 通道共用一个时钟，因此只有在两者拥有相同配置时，才可以实现全双工通信。


