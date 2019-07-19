# imapsan

## Introduction:

 This is a toy command line **IMAP** mail client. It mostly follows the older IMAP2 protocol, 
but also implements some features of the current IMAP protocol, IMAP4rev1.
 It has some basic features, like displaying messages by page, displaying the 
contents of a message, deleting messages, listing and selecting mailboxes. 



## Warnings:

 The LOGIN command is used for authentication, which is not supported by all clients.
Also SSL is not implemented, also excluding this application from connecting to
popular email services, such as Yahoo-Mail, or Gmail.
 **IMPORTANT:** The LOGIN command is not safe, as the data sent is not encrypted, please
DO NOT use this application if you fear for your email account's safety.

## How to install
```
make
```

## How to use:
Run with:
```
./imapsan <hostname> <port>
```


 Upon startup, you will be asked to enter your email account's username and password, and
if they are valid, you will proceed into the application menu, else you can
retry, or exit the application.

 Every message in your mailbox corresponds to an integer, this is the number that some of the commands bellow use.
 
 
 Once enter the menu, you may input any of the following commands:

+ **delete <num\>** - Marks the message with message number <num\> for deletion.
+ **undelete <num\>** - Unmarks the marked for deletion message with number <num\>. If not marked, it does nothing.
+ **expunge** - Deletes all messages that are marked for deletion.
+ **read <num\>** - Display the message with number <num\>.
+ **page <num\>** - Display all the messages on the page numbered <num\>.
+ **logout** - Close the connection with the server, and close the program.
+ **select <mailbox-name\>** - Select the mailbox named <mailbox-name\>. If it foes not exist,
      the user must choose another one, or if they stop trying, the selected mailbox
      defaults to inbox.
+ **list** - Lists all mailbox names the user can select
+ **stats** - Displays information about the mailbox, specifically, the total number
 of messages, recent messages, and the total number of pages (for use with page).
+ **help** - Prints most of this info inside the application.
+ **clear** - Clears the terminal's screen.

If you only want to use the program, feel free to stop here.



## Useful Resources:

  If you are familiar with UTF-8, Base 64 encoding, and the IMAP protocol, feel
 free to skip this section. If not, these links might be of help:

 **UTF-8:**
 	
    https://en.wikipedia.org/wiki/UTF-8  
 	
    https://tools.ietf.org/html/rfc3629

**Base 64:**
 	
    https://en.wikipedia.org/wiki/Base64
 	
    https://www.ietf.org/rfc/rfc2045.txt

 **IMAP:**
 	
    https://tools.ietf.org/html/rfc1176
 	
    https://tools.ietf.org/html/rfc3501



 ## To do:

 + The program does not detect a broken connection. Implement a timeout mechanism, so
  that if the server hasn't sent data for some second, the program frees all resources,
  and then tries to reconnect.

 + Implement a search feature (!search <criteria>), utilizing the SEARCH command, as
  defined in RFC 1176.

 + Implement a saving feature (!save <msg-number> <filepath), for storing messages
  on the disk.

 + Implement functionality for decoding UTF-8 stings in the Quoted-Printable encoding.
  Currently, such strings appear as "?? Unknown Encoding ??"

 + When a message is read all messages are cached, wasting a lot of memory. This must be changed to
  caching just the page in which the read message belongs.
