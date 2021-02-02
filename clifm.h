
				/** #######################
				 *  #  CLIFM CUSTOM LIB   # 
				 *  ######################*/


int
check_immutable_bit(char *file)
/* Check a file's immutable bit. Returns 1 if true, zero if false, and
 * -1 in case of error */
{
	#if !defined(FS_IOC_GETFLAGS) || !defined(FS_IMMUTABLE_FL)
	return 0;
	#else

	int attr, fd, immut_flag = -1;

	fd = open(file, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "'%s': %s\n", file, strerror(errno));
		return -1;
	}

	ioctl(fd, FS_IOC_GETFLAGS, &attr);
	if (attr & FS_IMMUTABLE_FL)
		immut_flag = 1;
	else
		immut_flag = 0;
	close(fd);

	if (immut_flag) 
		return 1;
	else 
		return 0;

	#endif /* !defined(FS_IOC_GETFLAGS) || !defined(FS_IMMUTABLE_FL) */
}

int
xgetchar(void)
/* Unlike getchar(), gets key pressed immediately, without the need to wait
 * for new line (Enter)
 * Taken from: 
 * https://stackoverflow.com/questions/12710582/how-can-i-capture-a-key-stroke-immediately-in-linux */
{
	struct termios oldt, newt;
	int ch;

	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	ch = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

	return ch;
}

int
xstrcmp(const char *str1, const char *str2)
{
/* Check for null. This check is done neither by strcmp nor by strncmp. I 
* use 256 for error code since it does not represent any ASCII code (the 
* extended version goes up to 255) */
	if (!str1 || !str2)
		return 256;

	while (*str1) {
		if (*str1 != *str2)
			return (*str1 - *str2);
		str1++;
		str2++;
	}
	if (*str2)
		return (0 - *str2);

	return 0;
	
/*	for (; *str1 == *str2; str1++, str2++)
		if (*str1 == 0x00)
			return 0;
	return *str1 - *str2; */
}

int
xstrncmp(const char *str1, const char *str2, size_t n)
{
	if (!str1 || !str2)
		return 256;

	size_t counter = 0;
	while (*str1 && counter < n) {
		if (*str1 != *str2)
			return (*str1 - *str2);
		str1++;
		str2++;
		counter++;
	}

	if (counter == n)
		return 0;
	if (*str2)
		return (0 - *str2);

	return 0;
}

char *
xstrcpy(char *buf, const char *str)
{
	if (!str)
		return (char *)NULL;

	while (*str) {
		*buf = *str;
		buf++;
		str++;
	}

	*buf = 0x00;

/*	while ((*buf++ = *str++)); */

	return buf;
}

char *
xstrncpy(char *buf, const char *str, size_t n)
{
	if (!str)
		return (char *)NULL;

	size_t counter = 0;
	while (*str && counter < n) {
		*buf = *str;
		buf++;
		str++;
		counter++;
	}

	*buf = 0x00;

/*	size_t counter = 0;
	while ((*buf++ = *str++) && counter++ < n); */

	return buf;
}

/*
size_t
xstrlen(const char *str)
{
	size_t len = 0;

	while (*(str++))
		len++;

	return len;
} */

/*
int
xatoi(const char *str)
// 2 times faster than atoi. Cannot handle negative number (See xnatoi below)
{
	int ret = 0; 
	
	while (*str) {
		if (*str < 0x30 || *str > 0x39)
			return ret;
		ret = (ret * 10) + (*str - '0');
		str++;
	}

	if (ret > INT_MAX)
		return INT_MAX;
	
	if (ret < INT_MIN)
		return INT_MIN;

	return ret;
}

int
xnatoi(const char *str)
// 2 times faster than atoi. The commented lines make xatoi able to handle 
// negative values
{
	int ret = 0, neg = 0;

	while (*str) {
		if (*str < 0x30 || *str > 0x39)
			return ret;
		ret = (ret * 10) + (*str - '0');
		str++;
	}

	if (neg)
		ret = ret - (ret * 2);

	if (ret > INT_MAX)
		return INT_MAX;
	
	if (ret < INT_MIN)
		return INT_MIN;

	return ret;
} */

pid_t
get_own_pid(void)
{
	pid_t pid;

	/* Get the process id */
	pid = getpid();

	if (pid < 0)
		return 0;
	else
		return pid;
}

char *
get_user(void)
/* Returns a pointer to a new string containing the current user's name, or
 * NULL if not found */
{
	struct passwd *pw;
	uid_t uid = 0;
	
	uid = geteuid();

	/* Get a passwd struct for current user ID. An alternative is
	 * to use setpwent(), getpwent(), and endpwent() */
	pw = getpwuid(uid);
/*
	printf("Username: %s\n", user_info->pw_name);
	printf("Password: %s\n", user_info->pw_passwd);
	printf("UID: %d\n", user_info->pw_uid);
	printf("GID: %d\n", user_info->pw_gid);
	printf("Gecos: %s\n", user_info->pw_gecos);
	printf("Home: %s\n", user_info->pw_dir);
	printf("Shell: %s\n", user_info->pw_shell);
*/

	if (!pw)
		return (char *)NULL;

	/* Why we don't just return a pointer to the field of the passwd struct
	 * we need? Because this struct will be overwritten by subsequent calls to
	 * getpwuid(), for example, in the properties function, in which case
	 * our pointer will point to a wrong string. So, to avoid this, we just
	 * copy the string we need into a new variable. The same applies to the
	 * following functions, get_user_home() and get_sys_shell() */
	char *p = (char *)NULL;
	p = (char *)malloc((strlen(pw->pw_name) + 1) * sizeof(char));

	if (!p)
		return (char *)NULL;
	
	char *username = p;
	p = (char *)NULL;

	strcpy(username, pw->pw_name);
	
	return username;
}

char *
get_user_home(void)
/* Returns a pointer to a string containing the user's home directory, or NULL
 * if not found */
{
	struct passwd *pw;
	
	pw = getpwuid(getuid());

	if (!pw)
		return NULL;

	char *p = (char *)NULL;
	p = (char *)malloc((strlen(pw->pw_dir) + 1) * sizeof(char));

	if (!p)
		return (char *)NULL;
	
	char *home = p;
	p = (char *)NULL;

	strcpy(home, pw->pw_dir);

	return home;
}

char *
get_sys_shell(void)
/* Returns a pointer to a string containing the user's default shell or NULL 
 * if not found */
{
	struct passwd *pw;

	pw = getpwuid(getuid());

	if (!pw)
		return (char *)NULL;

	char *p = (char *)NULL;
	p = (char *)malloc((strlen(pw->pw_shell) + 1) * sizeof(char));

	if (!p)
		return (char *)NULL;
	
	char *shell = p;
	p = (char *)NULL;

	strcpy(shell, pw->pw_shell);
	
	return shell;
}

int
is_number(const char *str)
/* Check whether a given string contains only digits. Returns 1 if true and 0 
 * if false. Does not work with negative numbers */
{
	for (; *str ; str++)
		if (*str < 0x30 || *str > 0x39)
			return 0;

	return 1;
}

size_t
digits_in_num(int num) {
/* Return the amount of digits in a given number */
	size_t count = 0; /* VERSION 2: neither printf nor any function call at all */

	while (num != 0) {
		num /= 10; /* n = n/10 */
		++count;
	}

	return count;
}

int
strcntchr(const char *str, const char c)
/* Returns the index of the first appearance of c in str, if any, and -1 if c 
 * was not found or if no str. NOTE: Same thing as strchr(), except that 
 * returns an index, not a pointer */
{
	if (!str)
		return -1;
	
	register int i = 0;

	while (*str) {
		if (*str == c)
			return i;
		i++;
		str++;
	}

	return -1;
}

char *
straft(char *str, const char c)
/* Returns the string after the first appearance of a given char, or returns 
 * NULL if C is not found in STR or C is the last char in STR. */
{
	if (!str || !*str || !c)
		return (char *)NULL;
	
	char *p = str, *q = (char *)NULL;

	while (*p) {
		if (*p == c) {
			q = p;
			break;
		}
		p++;
	}
	
	/* If C was not found or there is nothing after C */
	if (!q || !*(q + 1))
		return (char *)NULL;

	char *buf = (char *)malloc(strlen(q));

	if (!buf)
		return (char *)NULL;

	strcpy(buf, q + 1);

	return buf;
}

char *
straftlst(char *str, const char c)
/* Returns the string after the last appearance of a given char, or NULL if no 
 * match */
{
	if (!str || !*str || !c)
		return (char *)NULL;
	
	char *p = str, *q = (char *)NULL;
	
	while (*p) {
		if (*p == c)
			q = p;
		p++;
	}
	
	if (!q || !*(q + 1))
		return (char *)NULL;
	
	char *buf = (char *)malloc(strlen(q));
	
	if (!buf)
		return (char *)NULL;

	strcpy(buf, q + 1);
	
	return buf;
}

char *
strbfr(char *str, const char c)
/* Returns the substring in str before the first appearance of c. If not 
 * found, or C is the first char in STR, returns NULL */
{
	if (!str || !*str || !c)
		return (char *)NULL;

	char *p = str, *q = (char *)NULL;
	while (*p) {
		if (*p == c) {
			q = p; /* q is now a pointer to C */
			break;
		}
		p++;
	}
	
	/* C was not found or it was the first char in STR */
	if (!q || q == str)
		return (char *)NULL;

	*q = 0x00; 
	/* Now C (because q points to C) is the null byte and STR ends in C, which 
	 * is what we want */

	char *buf = (char *)malloc((size_t)(q - str + 1));

	if (!buf) { /* Memory allocation error */
		/* Give back to C its original value, so that STR is not modified in 
		 * the process */
		*q = c;
		return (char *)NULL;
	}

	strcpy(buf, str);
	
	*q = c;
	
	return buf;
}

char *
strbfrlst(char *str, const char c)
/* Get substring in STR before the last appearance of C. Returns substring 
 * if C is found and NULL if not (or if C was the first char in STR). */
{
	if (!str || !*str || !c)
		return (char *)NULL;

	char *p = str, *q = (char *)NULL;

	while (*p) {
		if (*p == c)
			q = p;
		p++;
	}
	
	if (!q || q == str)
		return (char *)NULL;
	
	*q = 0x00;

	char *buf = (char *)malloc((size_t)(q - str + 1));

	if (!buf) {
		*q = c;
		return (char *)NULL;
	}

	strcpy(buf, str);
	
	*q = c;
	
	return buf;
}

char *
strbtw(char *str, const char a, const char b)
/* Returns the string between first ocurrence of A and the first ocurrence of B 
 * in STR, or NULL if: there is nothing between A and B, or A and/or B are not 
 * found */
{
	if (!str || !*str || !a || !b)
		return (char *)NULL;

	char *p = str, *pa = (char *)NULL, *pb = (char *)NULL;

	while (*p) {
		if (!pa) {
			if (*p == a)
				pa = p;
		}
		else if (*p == b) {
			pb = p;
			break;
		}
		p++;
	}
	
	if (!pb)
		return (char *)NULL;
		
	*pb = 0x00;
	
	char *buf = (char *)malloc((size_t)(pb - pa));
	
	if (!buf) {
		*pb = b;
		return (char *)NULL;
	}
	
	strcpy(buf, pa + 1);
	
	*pb = b;
	
	return buf;
}

/* The following four functions (from_hex, to_hex, url_encode, and
 * url_decode) were taken from "http://www.geekhideout.com/urlcode.shtml"
 * and modified to comform to RFC 2395, as recommended by the freedesktop
 * trash specification */
char
from_hex(char c)
/* Converts a hex char to its integer value */
{
	return isdigit(c) ? c - '0' : tolower(c) - 'a' + 10;
}

char
to_hex(char c)
/* Converts an integer value to its hex form*/
{
	static char hex[] = "0123456789ABCDEF";
	return hex[c & 15];
}

char *
url_encode(char *str)
/* Returns a url-encoded version of str */
{
	if (!str || *str == 0x00)
		return (char *)NULL;

	char *p;
	p = (char *)calloc((strlen(str) * 3) + 1, sizeof(char));
	/* The max lenght of our buffer is 3 times the length of STR plus 1 extra 
	 * byte for the null byte terminator: each char in STR will be, if encoded, 
	 * %XX (3 chars) */
	if (!p)
		return (char *)NULL;

	char *buf;
	buf = p;
	p = (char *)NULL;

	/* Copies of STR and BUF pointers to be able
	* to increase and/or decrease them without loosing the original memory 
	* location */
	char *pstr, *pbuf; 
	pstr = str;
	pbuf = buf;
	
	for (; *pstr; pstr++) {
		if (isalnum (*pstr) || *pstr == '-' || *pstr == '_' 
					|| *pstr == '.' || *pstr == '~' || *pstr == '/')
			/* Do not encode the above chars */
			*pbuf++ = *pstr;
		else {
			/* Encode char to URL format. Example: space char to %20 */
			*pbuf++ = '%';
			*pbuf++ = to_hex(*pstr >> 4); /* Right shift operation */
			*pbuf++ = to_hex(*pstr & 15); /* Bitwise AND operation */
		}
	}
	
	return buf;
}

char *
url_decode(char *str)
/* Returns a url-decoded version of str */
{
	if (!str || str[0] == 0x00)
		return (char *)NULL;

	char *p = (char *)NULL;
	p = (char *)calloc(strlen(str) + 1, sizeof(char));
	/* The decoded string will be at most as long as the encoded string */

	if (!p)
		return (char *)NULL;

	char *buf;
	buf = p;
	p = (char *)NULL;
	
	char *pstr, *pbuf;
	pstr = str; 
	pbuf = buf;
	for ( ; *pstr; pstr++) {
		if (*pstr == '%') {
			if (pstr[1] && pstr[2]) {
				/* Decode URL code. Example: %20 to space char */  
				/* Left shift and bitwise OR operations */  
				*pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
				pstr += 2;
			}
		}
		else
			*pbuf++ = *pstr;
	}

	return buf;
}

char *
get_date (void)
{
	time_t rawtime = time(NULL);
	struct tm *tm = localtime(&rawtime);
	size_t date_max = 128;

	char *p = (char *)calloc(date_max + 1, sizeof(char)), *date;
	if (p) {
		date = p;
		p = (char *)NULL;
	}
	else
		return (char *)NULL;

	strftime(date, date_max, "%Y-%m-%dT%T%z", tm);

	return date;
}

char *
get_size_unit(off_t file_size)
/* Convert FILE_SIZE to human readeable form */
{
	size_t max_size_type_len = 11;
	/* Max size type length == 10 == "1024.1KiB\0",
	 * or 11 == "1023 bytes\0" */
	char *size_type = calloc(max_size_type_len + 1, sizeof(char));
	short units_n = 0;
	float size = (float)file_size;

	while (size > 1024) {
		size = size/1024;
		++units_n;
	}

	/* If bytes */
	if (!units_n)
		snprintf(size_type, max_size_type_len, "%.0f bytes", (double)size);
	else {
		static char units[] = { 'b', 'K', 'M', 'G', 'T', 'P', 'E', 'Z', 'Y' };		
		snprintf(size_type, max_size_type_len, "%.1f%ciB", (double)size, 
				 units[units_n]);
	}

	return size_type;
}

int
read_octal(char *str)
/* Convert octal string into integer. 
 * Taken from: https://www.geeksforgeeks.org/program-octal-decimal-conversion/ 
 * Used by decode_prompt() to make things like this work: \033[1;34m */
{
	if (!str)
		return -1;
	
	int n = atoi(str);
	int num = n; 
	int dec_value = 0; 

	/* Initializing base value to 1, i.e 8^0 */
	int base = 1; 

	int temp = num; 
	while (temp) { 

		/* Extracting last digit */
		int last_digit = temp % 10; 
		temp = temp / 10; 

		/* Multiplying last digit with appropriate 
		 * base value and adding it to dec_value */
		dec_value += last_digit * base; 

		base = base * 8; 
	}

	return dec_value; 
}

int
hex2int(char *str)
{
	int i, n[2];
	for (i = 1; i >= 0; i--) {
		if (str[i] >= '0' && str[i] <= '9')
			n[i] = str[i] - 0x30;
		else {
			switch(str[i]) {
			case 'A': case 'a': n[i] = 10; break;
			case 'B': case 'b': n[i] = 11; break;
			case 'C': case 'c': n[i] = 12; break;
			case 'D': case 'd': n[i] = 13; break;
			case 'E': case 'e': n[i] = 14; break;
			case 'F': case 'f': n[i] = 15; break;
			default: break;
			}
		}
	}

	return ((n[0] * 16) + n[1]);
}

char *
remove_quotes(char *str)
/* Removes end of line char and quotes (single and double) from STR. Returns a
 * pointer to the modified STR if the result is non-blank or NULL */
{
	if (!str || !*str)
		return (char *)NULL;
	
	char *p = str;
	size_t len = strlen(p);
	
	if (len > 0 && p[len - 1] == '\n') {
		p[len - 1] = 0x00;
		len--;
	}
	
	if (len > 0 && (p[len - 1] == '\'' || p[len - 1] == '"'))
		p[len - 1] = 0x00;

	if (*p == '\'' || *p == '"')
		p++;
	
	if (!*p)
		return (char *)NULL;
	
	char *q = p;
	int blank = 1;

	while(*q) {
		if (*q != 0x20 && *q != '\n' && *q != '\t') {
			blank = 0;
			break;
		}
		q++;
	}
	
	if (!blank)
		return p;
	
	return (char *)NULL;
}

int
is_acl(char *file)
/* Return 1 if FILE has some ACL property and zero if none
 * See: https://man7.org/tlpi/code/online/diff/acl/acl_view.c.html */
{
	if (!file || !*file)
		return 0;

	acl_t acl;
	int entryid, num = 0;
	acl = acl_get_file(file, ACL_TYPE_ACCESS);

	if (acl) {
		acl_entry_t entry;

		for (entryid = ACL_FIRST_ENTRY; ; entryid = ACL_NEXT_ENTRY) {
			if (acl_get_entry(acl, entryid, &entry) != 1)
				break;
			num++;
		}

		acl_free(acl);

		if (num > 3)
			/* We have something else besides owner, group, and others, that is,
			 * we have at least one ACL property */
			return 1;
		else
			return 0;
	}

	else /* Error */
		/* fprintf(stderr, "%s\n", strerror(errno)); */
		return 0;

	return 0;
}
