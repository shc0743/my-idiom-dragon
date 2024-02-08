<?php
require_once('init.php');
require_once('authapi.php');

if (!GetLoginState()) {
    header("Location: ./login.html");
    die();
}

header("Location: ./vapp.php");
