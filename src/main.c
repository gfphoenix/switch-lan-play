#include "lan-play.h"

// command-line options
struct cli_options options;

OPTIONS_DEF(socks5_server_addr);
OPTIONS_DEF(relay_server_addr);
uv_signal_t signal_int;

#define DEFAULT_NETIF_IPCIDR  "10.5.0.1/16"
int list_interfaces(pcap_if_t *alldevs)
{
    int i = 0;
    pcap_if_t *d;
    for (d = alldevs; d; d = d->next) {
        printf("%d. %s", ++i, d->name);
        if (d->description) {
            printf(" (%s)", d->description);
        } else {
            printf(" (No description available)");
        }
        if (d->addresses) {
            struct pcap_addr *taddr;
            struct sockaddr_in *sin;
            char  revIP[100];
            bool  first = true;
            for (taddr = d->addresses; taddr; taddr = taddr->next)
            {
                sin = (struct sockaddr_in *)taddr->addr;
                if (sin->sin_family == AF_INET) {
                    strncpy(revIP, inet_ntoa(sin->sin_addr), sizeof(revIP));
                    if (first) {
                        printf("\n\tIP: [");
                        first = false;
                    } else {
                        putchar(',');
                    }
                    printf("%s", revIP);
                }
            }
            if (!first) {
                putchar(']');
            }
        }
        putchar('\n');
    }
    return i;
}

static int parse_ipcidr(struct cli_options *options)
{
    char *ipcidr = options->netif_ipcidr;
    char temp[64];
    char *p;
    char *endp;
    int prelen;
    size_t n;
    uint32_t subnet;
    uint32_t mask;
    if (!ipcidr) {
        fprintf(stderr, "netif_ipcidr is NULL\n");
        return -1;
    }
    n = strlen(ipcidr);
    if (n > 50) {
        fprintf(stderr, "Invalid ipcidr = '%s'\n", ipcidr);
        return -1;
    }
    memcpy(temp, ipcidr, n + 1);
    p = strchr(temp, '/');
    if (!p) {
        fprintf(stderr, "Invalid ipcider = '%s', doesn't contain '/'\n", ipcidr);
        return -1;
    }
    *p = '\0';
    p++;
    if (uv_inet_pton(AF_INET, temp, &subnet) != 0) {
        fprintf(stderr, "Invalid IPv4 address = '%s'\n", temp);
        return -1;
    }
    subnet = ntohl(subnet);
    snprintf(options->netif_ipaddr, sizeof(options->netif_ipaddr), "%u.%u.%u.%u",
             (uint8_t)(subnet >> 24), (uint8_t)(subnet >> 16),
             (uint8_t)(subnet >> 8), (uint8_t)(subnet >> 0));

    prelen = strtol(p, &endp, 10);
    if (*endp != '\0') {
        fprintf(stderr, "Invalid prefix = '%s', endptr='%s'\n", p, endp);
        return -1;
    }
    if (prelen < 0 || prelen > 32) {
        fprintf(stderr, "Invalid prefix len = %d\n", prelen);
        return -1;
    }
    mask = prelen ? ~((1 << (32 - prelen)) - 1) : 0;

    snprintf(options->netif_netmask, sizeof(options->netif_netmask), "%u.%u.%u.%u",
             (uint8_t)(mask >> 24), (uint8_t)(mask >> 16),
             (uint8_t)(mask >> 8), (uint8_t)(mask >> 0));

    subnet = subnet & mask;
    snprintf(options->netif_netaddr, sizeof(options->netif_netaddr), "%u.%u.%u.%u",
             (uint8_t)(subnet >> 24), (uint8_t)(subnet >> 16),
             (uint8_t)(subnet >> 8), (uint8_t)(subnet >> 0));
    return 0;
}
int parse_arguments(int argc, char **argv)
{
    #define CHECK_PARAM() if (1 >= argc - i) { \
        eprintf("%s: requires an argument\n", arg); \
        return -1; \
    }
    if (argc <= 0) {
        return -1;
    }

    options.help = 0;
    options.version = 0;

    options.broadcast = false;
    options.pmtu = 0;
    options.fake_internet = false;
    options.list_if = false;

    options.netif = NULL;
    options.netif_ipcidr = DEFAULT_NETIF_IPCIDR;
    options.netif_ipaddr[0] = '\0';
    options.netif_netmask[0] = '\0';
    options.netif_netaddr[0] = '\0';

    options.relay_server_addr = NULL;
    options.relay_username = NULL;
    options.relay_password = NULL;
    options.relay_password_file = NULL;

    options.socks5_server_addr = NULL;
    options.socks5_username = NULL;
    options.socks5_password = NULL;
    options.socks5_password_file = NULL;
    options.rpc = NULL;
    options.rpc_token = NULL;
    options.rpc_protocol = NULL;

    int i;
    for (i = 1; i < argc; i++) {
        char *arg = argv[i];

        if (!strcmp(arg, "--help")) {
            options.help = 1;
        } else if (!strcmp(arg, "--version")) {
            options.version = 1;
        } else if (!strcmp(arg, "--netif")) {
            CHECK_PARAM();
            options.netif = strdup(argv[i + 1]);
            i++;
        } else if (!strcmp(arg, "--netif-ipcidr")) {
             CHECK_PARAM();
             options.netif_ipcidr = argv[i + 1];
             i++;
        } else if (!strcmp(arg, "--relay-server-addr")) {
            CHECK_PARAM();
            options.relay_server_addr = argv[i + 1];
            i++;
        } else if (!strcmp(arg, "--username")) {
            CHECK_PARAM();
            options.relay_username = argv[i + 1];
            i++;
        } else if (!strcmp(arg, "--password")) {
            CHECK_PARAM();
            options.relay_password = argv[i + 1];
            i++;
        } else if (!strcmp(arg, "--password-file")) {
            CHECK_PARAM();
            options.relay_password_file = argv[i + 1];
            i++;
        } else if (!strcmp(arg, "--socks5-server-addr")) {
            CHECK_PARAM();
            options.socks5_server_addr = argv[i + 1];
            i++;
        // } else if (!strcmp(arg, "--socks5-username")) {
        //     CHECK_PARAM();
        //     options.socks5_username = argv[i + 1];
        //     i++;
        // } else if (!strcmp(arg, "--socks5-password")) {
        //     CHECK_PARAM();
        //     options.socks5_password = argv[i + 1];
        //     i++;
        // } else if (!strcmp(arg, "--socks5-password-file")) {
        //     CHECK_PARAM();
        //     options.socks5_password_file = argv[i + 1];
        //     i++;
        } else if (!strcmp(arg, "--list-if")) {
            options.list_if = true;
        } else if (!strcmp(arg, "--broadcast")) {
            options.broadcast = true;
            options.relay_server_addr = "255.255.255.255:11451";
        } else if (!strcmp(arg, "--pmtu")) {
            CHECK_PARAM();
            options.pmtu = atoi(argv[i + 1]);
            i++;
        } else if (!strcmp(arg, "--fake-internet")) {
            options.fake_internet = true;
        } else if (!strcmp(arg, "--set-ionbf")) {
            setvbuf(stdout, NULL, _IONBF, 0);
            setvbuf(stderr, NULL, _IONBF, 0);
        } else if (!strcmp(arg, "--rpc")) {
            CHECK_PARAM();
            options.rpc = argv[i + 1];
            i++;
        } else if (!strcmp(arg, "--rpc-token")) {
            CHECK_PARAM();
            options.rpc_token = argv[i + 1];
            i++;
        } else if (!strcmp(arg, "--rpc-protocol")) {
            CHECK_PARAM();
            options.rpc_protocol = argv[i + 1];
            i++;
        } else {
            LLOG(LLOG_WARNING, "unknown paramter: %s", arg);
        }
    }

    if (options.help || options.version || options.list_if || options.rpc) {
        return 0;
    }
    if (parse_ipcidr(&options) != 0)
        return -1;

    if (!options.relay_server_addr) {
        if (options.socks5_server_addr) {
            options.relay_server_addr = "127.0.0.1:11451";
        } else {
            eprintf("--relay-server-addr is required\n");
        }
        // return -1;
    }
    if (options.socks5_username) {
        if (!options.socks5_password && !options.socks5_password_file) {
            eprintf("username given but password not given\n");
            return -1;
        }

        if (options.socks5_password && options.socks5_password_file) {
            eprintf("--password and --password-file cannot both be given\n");
            return -1;
        }
    }
    if (options.relay_username) {
        if (!options.relay_password && !options.relay_password_file) {
            eprintf("username given but password not given\n");
            return -1;
        }

        if (options.relay_password && options.relay_password_file) {
            eprintf("--password and --password-file cannot both be given\n");
            return -1;
        }
    }

    return 0;
}

void print_help(const char *name)
{
    printf(
        "Usage:\n"
        "    %s\n"
        "        [--help]\n"
        "        [--version]\n"
        "        [--broadcast]\n"
        "        [--fake-internet]\n"
        "        [--netif <interface>] default: all\n"
        "        [--netif-ipcidr <IP/prefix> default: " DEFAULT_NETIF_IPCIDR "\n"
        "        [--relay-server-addr <addr>]\n"
        "        [--username <username>]\n"
        "        [--password <password>]\n"
        "        [--password-file <password-file>]\n"
        "        [--list-if]\n"
        "        [--pmtu <pmtu>]\n"
        "        [--socks5-server-addr <addr>]\n"
        "        [--rpc <address>]\n"
        "        [--rpc-token <token>]\n"
        "        [--rpc-protocol <rpc protocl>]\n"
        // "        [--socks5-username <username>]\n"
        // "        [--socks5-password <password>]\n"
        // "        [--socks5-password-file <file>]\n"
        "Address format is a.b.c.d:port (IPv4).\n"
        "RPC protocol could be tcp, ws. Default to ws.\n",
        name
    );
}

void walk_cb(uv_handle_t* handle, void* arg)
{
    if (!uv_is_closing(handle)) {
        uv_close(handle, NULL);
    }
    // LLOG(LLOG_DEBUG, "walk %d %p", handle->type, handle->data);
}

void lan_play_signal_cb(uv_signal_t *signal, int signum)
{
    struct lan_play *lan_play = signal->data;
    eprintf("stopping signum: %d\n", signum);

    int ret = lan_play_close(lan_play);
    if (ret) {
        LLOG(LLOG_ERROR, "lan_play_close %d", ret);
    }

    ret = uv_signal_stop(&signal_int);
    if (ret) {
        LLOG(LLOG_ERROR, "uv_signal_stop(signal_int) %d", ret);
    }

    uv_close((uv_handle_t *)&signal_int, NULL);

    uv_walk(lan_play->loop, walk_cb, lan_play);
}

void print_version(const char *name)
{
    printf("%s " LANPLAY_VERSION "\n", name);
}

void list_netif()
{
    pcap_if_t *alldevs;
    char err_buf[PCAP_ERRBUF_SIZE];

    if (pcap_findalldevs(&alldevs, err_buf)) {
        fprintf(stderr, "Error pcap_findalldevs: %s\n", err_buf);
        exit(1);
    }

    list_interfaces(alldevs);

    pcap_freealldevs(alldevs);
}

int old_main()
{
    char relay_server_addr[128] = { 0 };
    struct lan_play *lan_play = &real_lan_play;
    int ret;

    lan_play->loop = uv_default_loop();

    if (options.list_if) {
        list_netif();
        return 0;
    }

    if (options.relay_server_addr == NULL) {
        printf("Input the relay server address [ domain/ip:port ]:");
        scanf("%100s", relay_server_addr);
        options.relay_server_addr = relay_server_addr;
    }

    if (parse_addr(options.relay_server_addr, &lan_play->server_addr) != 0) {
        LLOG(LLOG_ERROR, "Failed to parse and get ip address. --relay-server-addr: %s", options.relay_server_addr);
        return -1;
    }

    RT_ASSERT(uv_signal_init(lan_play->loop, &signal_int) == 0);
    RT_ASSERT(uv_signal_start(&signal_int, lan_play_signal_cb, SIGINT) == 0);
    signal_int.data = lan_play;

    if (options.netif == NULL) {
        printf("Interface not specified, opening all interfaces\n");
    } else {
        printf("Opening single interface\n");
    }
    RT_ASSERT(lan_play_init(lan_play) == 0);

    ret = uv_run(lan_play->loop, UV_RUN_DEFAULT);
    if (ret) {
        LLOG(LLOG_ERROR, "uv_run %d", ret);
    }

    LLOG(LLOG_DEBUG, "lan_play exit %d", ret);

    return ret;
}

int main(int argc, char **argv)
{
    if (parse_arguments(argc, argv) != 0) {
        LLOG(LLOG_ERROR, "Failed to parse arguments");
        print_help(argv[0]);
        return 1;
    }
    if (options.version) {
        print_version(argv[0]);
        return 0;
    }
    if (options.help) {
        print_version(argv[0]);
        print_help(argv[0]);
        return 0;
    }
    if (options.rpc) {
        return rpc_main(options.rpc, options.rpc_token, options.rpc_protocol);
    } else {
        return old_main();
    }
}
