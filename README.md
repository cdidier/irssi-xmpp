irssi-xmpp
==========

**Homepage:** https://github.com/cdidier/irssi-xmpp


## About ##

irssi-xmpp is an irssi plugin to connect to the XMPP network (jabber).

Its main features are:
 * Sending and receiving messages in irssi's query windows
 * A roster with contact & resource tracking (contact list)
 * Contact management (add, remove, manage subscriptions)
 * MUC (Multi-User Chat)
 * Tab completion of commands, JIDs and resources
 * Message Events ("composing")
 * Support for multiple accounts
 * Unicode support (UTF-8)
 * StartTLS, SSL (deprecated) and HTTP proxy support
 * ...

To deal with the XMPP protocol, it uses of the Loudmouth library.
Written in C and released under the GNU General Public License version 2.


## Installation ##

### Requirement ###
 * Loudmouth (>= 1.4.x): https://github.com/mcabber/loudmouth
 * Irssi (>= 0.8.13) and its sources unpacked and configured:
      http://irssi.org/

### Procedure ###
 * edit the file **config.mk** if needed and export this environment variable:
   ``` $ export IRSSI_INCLUDE=/path/to/irssi/sources ```
 * build the sources:
   ```$ make ```
 * install the module:
   *  in your home directory:
     ``` $ make user-install ```
   * in the base system:
    ``` # make install ```

### Packages ###
  * Debian/Ubuntu package: ``` apt-get install irssi-plugin-xmpp ``` ([more info](https://packages.debian.org/sid/irssi-plugin-xmpp))
  * OpenBSD port: ``` pkg_add irssi-xmpp ``` ([more info](http://openports.se/net/irssi-xmpp))
  * FreeBSD port: ``` pkg_add -r irssi-xmpp ``` ([more info](http://www.freshports.org/irc/irssi-xmpp/))
  * MacOS Homebrew: ``` brew install simmel/irssi/irssi-xmpp ``` ([more info](https://github.com/simmel/homebrew-irssi))
  * and in many package repository of Linux distributions...

## Documentation ##

In the directory **docs/**:
 * **STARTUP**: Getting started
 * **GENERAL**: How to use irssi-xmpp and related commands
 * **MUC**: How to use Multi-User Chat and related commands
 * **FAQ**: Frequently Asked Questions and Troubleshooting
 * **XEP**: XMPP Extensions supported
 * **INTERNAL**: How irssi-xmpp works

In the directory **help/** you can find the help files of each irssi-xmpp
specific commands, which can be viewed in irssi with the command /HELP.


## Bugs and suggestions ##

 * On Github:	https://github.com/cdidier/irssi-xmpp/issues

 * MUC room:	irssi-xmpp@chat.jabberfr.org

If irssi crashes, please build irssi with debug symbols and the module
irssi-xmpp in debug mode (take a look at **config.mk** to activate it).
Then you can run irssi in gdb and print the backtrace ("bt full") when
irssi crashes.  Paste the backtrace in your message would help to fix
this bug.
