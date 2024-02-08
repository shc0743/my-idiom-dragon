<?php

require_once('init.php');
require_once('authapi.php');



if (!GetLoginState()) {
    header("Location: ./login.html");
    die();
}


if (!isset($_REQUEST['action'])) {
    header("HTTP/1.1 400 ");
    die();
}
$act = $_REQUEST['action'];


function getfn($u) {
    return 'data/sessions/' . $u . ".session.php";
}
function getsesn($sid) {
    return 'data/sessions/sess-' . $sid . ".php";
}
function hasSess($sid) {
    return file_exists('data/sessions/sess-' . $sid . ".php");
}


function insertContentToFile($file, $lineNumber, $newContent) {

    $lines = file($file);
 
    $lines[$lineNumber - 1] = trim($lines[$lineNumber - 1]). $newContent ."\n";

    // 将修改后的内容写回文件  
    file_put_contents($file, implode("", $lines)); 
}


if ($act == "getdata" || $act == "getid" || $act == "getplayers") {
    $fn = getfn($_SESSION['username']);
    if (!isset($_SESSION['sessid'])) {
        if (file_exists($fn)) {
            $fp = fopen($fn, "r");
            fgets($fp);
            $str = trim(fgets($fp));
            $_SESSION['sessid'] = $str;
            fclose($fp);
        } else {
            header("HTTP/1.1 404 "); die();
        }
    }
    $fp = fopen($fn, "r");
    if (!$fp) {
        header("HTTP/1.1 404 "); die();
    }
    fgets($fp); $sid = trim(fgets($fp));
    if ($act == "getid") {
        echo($sid); die();
    }
    fclose($fp);

    $fp = fopen(getsesn($sid), "r");
    fgets($fp);
    if ($act == "getplayers") {
        header('content-type: text/plain;charset=utf-8');
        echo(fgets($fp)); die();
    }
    fgets($fp);

    header('content-type: text/plain;charset=utf-8');
    while ($str = fgets($fp)) {
        echo($str);
    }
    die();
}


if ($act == "create") {
    if (isset($_SESSION['sessid']) && file_exists(getsesn($_SESSION['sessid']))) {
        header("HTTP/1.1 400 "); die();
    }
    $u = $_SESSION['username'];
    
    $fpath = getfn($u);
    if (file_exists($fpath)) {
        header("HTTP/1.1 400 "); die();
    }
    
    $n = 0;
    while (true) {
        ++$n;
        $sid = '2'.strval(rand(10000000, 99999999));
        if (hasSess($sid)) {
            if ($n > 100000) {
                header("HTTP/1.1 500 "); die();
            }
            continue;
        }
        break;
    }

    $fp = fopen($fpath, "w");
    fputs($fp, "<?php die(); ?>\n");
    fputs($fp, $sid."\n");
    fclose($fp);
    $fp = fopen(getsesn($sid), "w");
    fputs($fp, "<?php die(); ?>\n");
    fputs($fp, $u.",\n{}\n"); // 第二行记录所有参与者
    fclose($fp);
    $_SESSION['sessid'] = ($sid);

    echo($sid);

    die();
}


if ($act == "join") {
    if (isset($_SESSION['sessid']) && file_exists(getsesn($_SESSION['sessid']))) {
        header("HTTP/1.1 400 "); die();
    }
    $u = $_SESSION['username'];

    $fpath = getfn($u);
    if (file_exists($fpath)) {
        header("HTTP/1.1 400 "); die();
    }
    
    $sid = isset($_REQUEST['sid']) ? $_REQUEST['sid'] : null;
    if (!$sid) {
        header("HTTP/1.1 400 "); die();
    }

    if (!file_exists(getsesn($sid))) {
        header("HTTP/1.1 404 "); die();
    }
    
    $fp = fopen($fpath, "w");
    fputs($fp, "<?php die(); ?>\n");
    fputs($fp, $sid."\n");
    fclose($fp);
    insertContentToFile(getsesn($sid), 2, $u.",");
    $_SESSION['sessid'] = ($sid);

    echo($sid);

    die();
}



if ($act == 'checkAccountPunishType') {
    if ($_SESSION['username'] != '妈妈' && $_SESSION['username'] != '爸爸') {
        echo '<a target=_blank href="https://genshin.hoyoverse.com/en/character/Fontaine?char=6">芙宁娜</a>';
    }
    else echo '奖励';
    die();
}



header("HTTP/1.1 400 "); die();
