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
    <script type="importmap">{
      "imports": {
        "@/": "./",
        "vue": "./modules/vue/vue.esm-browser.js",
        "element-plus": "./modules/element-plus/index.full.mjs.js",
        "element-plus/": "./modules/element-plus/",
        "icons-vue": "./modules/element-plus/icons-vue.min.js",
        "util/": "./modules/util/"
      }
    }</script>
    <link rel="stylesheet" href="./vapp.css">
</head>

<body>
    <div id="app"></div>
    <div id="myApp"></div>

    <dialog id="appError">
        <form method=dialog>
            <h1 style="margin-top: 0; color: red"> 错误！</h1>
            <pre style="min-width: 300px; min-height: 200px; border: 1px solid;"></pre>
            <button type=submit class="el-button el-button--primary is-plain">关闭</button>
        </form>
    </dialog>

    <div id="debugInfo"></div>

    <script src="./modules/es-module-shims/es-module-shims.js"></script>
    <script type="module" src="./vapp.esm.js"></script>
</body>

</html>