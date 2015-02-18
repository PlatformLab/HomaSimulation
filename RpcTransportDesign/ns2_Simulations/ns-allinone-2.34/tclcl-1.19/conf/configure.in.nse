dnl autoconf rules for NS Emulator (NSE)
dnl $Id: configure.in.nse,v 1.3 2000/03/10 01:49:32 salehi Exp $

dnl
dnl Look for ethernet.h
dnl

dnl Now look for supporting structures
dnl

AC_MSG_CHECKING([for struct ether_header])
AC_TRY_COMPILE([
#include <stdio.h>
#include <net/ethernet.h>
], [
int main()
{
	struct ether_header etherHdr;

	return 1;
}
], [
AC_DEFINE(HAVE_ETHER_HEADER_STRUCT)
AC_MSG_RESULT(found)
], [
AC_MSG_RESULT(not found)
])
 

dnl 
dnl Look for ether_addr
dnl
AC_MSG_CHECKING([for struct ether_addr])
AC_TRY_COMPILE([
#include <stdio.h>
#include <net/ethernet.h>
], [
int main()
{
	struct ether_addr etherAddr;

	return 0;
}
], [
AC_DEFINE(HAVE_ETHER_ADDRESS_STRUCT)
AC_MSG_RESULT(found)
], [
AC_MSG_RESULT(not found)
])

cross_compiling=no
dnl
dnl Look for addr2ascii function
dnl
AC_CHECK_FUNCS(addr2ascii)

dnl
dnl look for SIOCGIFHWADDR
dnl
AC_TRY_RUN(
#include <stdio.h>
#include <sys/ioctl.h>
int main()
{
	int i = SIOCGIFHWADDR;
	return 0;
}
, AC_DEFINE(HAVE_SIOCGIFHWADDR), , echo 1
)

