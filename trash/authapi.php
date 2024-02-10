<?php
require_once('init.php');


function GetLoginState() {
    if (!isset($_SESSION['isLogin'])) return false;
    return !!$_SESSION['isLogin'];    
}
function Login($username, $tok) {
    if (!$username || $username == '') return false;
    if (!$tok || $tok == '') return false;
    $tok = hash("sha256", $tok);
    $fp = fopen("users.php", "r");
    fgets($fp);
    while ($str = fgets($fp)) {
        $str = trim($str);
        if (!$str || $str == '') continue;
        $unpos = strpos($str, "=");
        $u = substr($str, 0, $unpos);
        $p = substr($str, $unpos + 1);
        // echo("username=".$u.",p=".$p."\n");
        if ($username == $u && $tok == $p) {
            $_SESSION['isLogin'] = true;
            $_SESSION['username'] = $u;
            return true;
        }
    }
    return false;
}
function Logout() {
    session_destroy();
    session_regenerate_id();
    return true;
}


