#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

int main(int argc, char **argv) {

	FILE *fd = NULL;
	openlog(NULL,0,LOG_USER);

	if ( argc != 3 ) {
		syslog(LOG_ERR, "no args");
		closelog();
		exit(1);
	}


	const char *pcPath=argv[1];
	const char *pcText=argv[2];

	syslog(LOG_DEBUG, "Writing %s to %s", pcPath, pcText);

	fd = fopen(pcPath, "w");

	if ( fd == NULL ) {
		syslog(LOG_ERR, "%s\n",strerror(errno));
		closelog();
		exit(1);
	}

	fprintf(fd, "%s", pcText);
	fclose(fd);
	closelog();

}
