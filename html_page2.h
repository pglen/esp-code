

/* =====[ AKOSTAR WiFi Clock project ]====================================

   File Name:       main/page_2.html
   
   Description:     Main file

   Revisions:

      REV   DATE            BY              DESCRIPTION
      ----  -----------     ----------      ------------------------------
      0.00  mar.07.2018     Peter Glen      Initial version.

   ======================================================================= */

// Page contents

const char scan_html [] = 

"<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\">"
"<html>"
"<title>WiFiClock Config</title>"
"</head>"
"<body>"
"<center>"
"<h1>WiFiClock Connection Configuration</h1>"
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
"<tr><td colspan=2 align=center>"
"The following fields determine  which AP to connect to "
"for acquiring time."
"<tr><td colspan=2 align=center height=12>"
"<tr> <td>"
"Select Access Point:&nbsp  "
"<td>"
"<select name=apname>"
"    <option value=\"AP-NUL\"> Access_Point_String_0 </option> "
"    <option value=\"AP-ONE\"> Access_Point_String_1 </option> "
"    <option value=\"AP-TWO\"> Access_Point_String_2 </option> "
"    <option value=\"AP-THR\"> Access_Point_String_3 </option> "
"    <option value=\"AP-FOU\"> Access_Point_String_4 </option> "
"    <option value=\"AP-FIV\"> Access_Point_String_5 </option> "
"    <option value=\"AP-SIX\"> Access_Point_String_6 </option> "
"    <option value=\"AP-SEV\"> Access_Point_String_7 </option> "
"    <option value=\"AP-EIG\"> Access_Point_String_8 </option> "
"    <option value=\"AP-NIN\"> Access_Point_String_9 </option> "
"</select>"
"</tr>"
"<tr><td>"
"AP Password: <td><input name=cpass type=password> &nbsp"
"</tr>"
"<tr><td colspan = 2>"
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
