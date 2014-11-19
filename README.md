Gmail Scope for Unity8
======================
It's the new hip way to read email.  This scope is in an early-beta
state.  Some things may break.  Use with caution.

Building
--------
You will need the [Ubuntu SDK][1].  To build out-of-tree, do
```
$ mkdir <build directory>
$ cd <build directory>
$ cmake <path to source>
$ make
```
Note that Qt Creator will do this automatically for you.

Google Credentials
------------------
This scope needs a client id and a client secret from Google in order to
connect to the Gmail API.  As per the Google Terms of Service, these
cannot be included in this repository.  To compile and run this scope,
you will need to obtain your own credentials.  [Google][2] can lead you
through the process, or you can:

1. Create a new project from the [Google Developers Console][3].
2. Select *APIs & auth* > *APIs*.
3. Enable the Gmail API.
4. Select *APIs & auth* > *Credentials* and click *Create new Client ID*.
5. Add a product name to your consent screen and click *Save*.
6. In the Create Client ID box that pops up, choose *Web application* as the type.
7. Enter `https://wiki.ubuntu.com` for both the *Authorized Javascript Origins* and *Authorized Redirect URIs*.  (I don't know if this is strictly necessary, but that's how it's set for me.)
8. Click *Create Client ID*.
9. Copy the *Client ID* and *Client Secret* into the file `gmail.secret` in the root of the repository.  These should appear in that order on their own lines, replacing the placeholder text.

Known Problems
--------------
Known bugs are listed on the [issue tracker][4].  If you don't see
your problem listed there, please add it!

License
-------
The Gmail Scope is licensed under the GPL v3 or later.  See the file
COPYING for more details.


[1]: http://developer.ubuntu.com/start/ubuntu-sdk/installing-the-sdk/ "Ubuntu SDK"
[2]: https://developers.google.com/gmail/api/auth/web-server#create_a_client_id_and_client_secret
[3]: https://console.developers.google.com/
[4]: https://github.com/rschroll/gmail-scope/issues "Bug tracker"
