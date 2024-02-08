<?php
require_once('init.php');
require_once('authapi.php');

if (!GetLoginState()) {
    header("Location: ./");
    die();
}

?>

<!DOCTYPE html>
<html lang="zh-cn">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
    <title>成语接龙</title>
    <meta name="description" content="成语接龙">
    <link rel="stylesheet" href="app.css">
    <link rel="stylesheet" href="element-plus.css">
</head>

<body>

    <dialog id="dlg">
        <div id="caption">
            成语接龙
        </div>
    </dialog>

    <div id="app">
        <resizable-widget id=myApp open>
            <div slot="widget-caption">
                <span id="usernameContainer"></span>成语接龙
                <a href="logout.php" data-exclude-bindmove style="position: absolute; right: 1em;">退出</a>
            </div>

            <main>
                <div id="selectSessionWizard" hidden class=xbig>
                    <div class="bold">创建会话...</div>
                    <button type="button" class="el-button el-button--primary is-plain" id=createSessBtn>立即创建</button>
                    <hr>
                    <div class="bold">加入会话...</div>
                    <form id="frmAddSess" style="display: flex; align-items: center;">
                        <input type="text" id="addSess_sid" required style="flex: 1" placeholder="会话 ID...">
                        <button type="submit" class="el-button el-button--primary is-plain" style="margin-left: 0.5em">立即加入</button>
                    </form>
                </div>

                <div hidden id="iSess">
                    <div style="text-align: center; user-select: none;">会话 ID: <span id="sidDisp"></span><a id="endSession" hidden style="margin-left: 0.5em;" href="./session.php?action=end&redirect=true">结束</a></div>
                    <div><div id="playersView"></div></div>
                    <div style="flex: 1" id="mainInputArea">
                        <div id="inputBox">
                            <input type="text">
                        </div>
                    </div>
                </div>
            </main>
        </resizable-widget>
    </div>

    <dialog id="appError">
        <form method=dialog>
            <h1 style="margin-top: 0; color: red"> 错误！</h1>
            <pre style="min-width: 300px; min-height: 200px; border: 1px solid;"></pre>
            <button type=submit class="el-button el-button--primary is-plain">关闭</button>
        </form>
    </dialog>

    <div id="debugInfo">Session ID: <?php echo ($_COOKIE['IdiomDragonSession-876e09f7f5ef4ef789843471ae11f544']); ?></div>

    <script type="module" src="./app.esm.js"></script>
</body>

</html>