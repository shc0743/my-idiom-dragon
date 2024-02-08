<?php
require_once('init.php');
require_once('authapi.php');

if (!GetLoginState()) {
    header("Location: ./login.html");
    die();
}

header('content-type: text/plain;charset=utf-8');
echo $_SESSION['username'];

