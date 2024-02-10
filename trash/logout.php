<?php
require_once('init.php');
require_once('authapi.php');

if (GetLoginState()) {
    Logout();
}



header("Location: ./");
die();
