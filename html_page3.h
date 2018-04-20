

/* =====[ AKOSTAR WiFi Clock project ]====================================

   File Name:       main/page_3.html
   
   Description:     Main file

   Revisions:

      REV   DATE            BY              DESCRIPTION
      ----  -----------     ----------      ------------------------------
      0.00  mar.07.2018     Peter Glen      Initial version.

   ======================================================================= */

// Page contents

const char station_html [] = 

"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\">"
"<html>"
"<title>WiFiClock Config</title>"
"</head>"
"<body>"
"<center>"
"<h1>WiFiClock Station Name Config</h1>"
"</center>"
"<style>"
".container {"
"	text-align: left;"
"	border: 1px solid green;"
"	padding: 5px;"
"}"
".container table {"
"	border: 1px solid red;        "
"    margin: 0 auto;"
"    }"
"</style>"
"<form method=post>"
"<table align=center>"
"<tr><td colspan=2 align=center height=12>"
"The following fields determine the station name of this WiFi Clock. The"
"<br>"
"clock will appear on your network by this name, and can be configured"
" with a password."
"<br>"
"The passord  must be at least <font color=red> 8 characters</font> "
"or longer. "
"<br>"
"Please avoid using space and punctuation marks."
"<tr><td colspan=2 align=center height=12>"
"<tr> <td align=right>"
"Station name:  &nbsp<td><input name=stname type=text value=station_name_here>"
"<tr><td align=right>"
"Station password:  &nbsp<td><input name=spass type=password>"
"</tr>"
"<tr><td align=right>"
"Repeat password:  &nbsp<td><input name=spass2 type=password>"
"</tr>"
"<tr><td colspan=2 align=center height=12>"
"<tr><td colspan=2>"
"<hr>"
"<tr><td colspan=2 align=center>"
"<div style=\"font-size:24px\">"
"<b>Clock Time at last Page Load:</b>"
"</div>"
"<tr><td colspan=2 align=center>"
"<div style=\"font-size:100px\">"
"00:00"
"</div>"
"<tr><td colspan=2 align=center>"
"<tr><td colspan=2 align=center>"
"Last updated at: dd/mm/yyyy hh:mm"
"<tr><td colspan=2 align=center>"
"<tr><td colspan=2>"
"<hr>"
"<tr><td colspan=2 align=center height=12>"
"</tr>"
"<tr><td colspan=2 align=center>"
"<input type=submit value='Save Configuration'>"
"</tr>"
"<tr><td colspan=2 height=12>"
"<tr><td colspan=2 align=center>"
"<div style=\"font-size:large\">"
"<a href=page_1.html?home=true>Return to WiFi Clock Home Page.</a>"
"</div>"
"<tr><td colspan=2 height=12>"
"</table>"
"</form>"
"</body>"
"</html>"
;
// EOF
