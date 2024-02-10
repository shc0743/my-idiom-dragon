<?php
require_once('init.php');
require_once('authapi.php');



if (isset($_POST['token']) && $_POST['token'] &&
    isset($_POST['user']) && $_POST['user']) {
    $r = $_POST['user']; $t = $_POST['token'];
    if (Login($r, $t)) {
        if (isset($_REQUEST['autoredirect'])) {
            header("Location: ./");
        }
        else header('HTTP/1.1 204 ');
    } else {
        header('HTTP/1.1 401 ');
    }
    die();
}




if (!GetLoginState()) {
    header('HTTP/1.1 401 '); die();
}


