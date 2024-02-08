<form action method=POST>
<textarea name=data><?php
if (isset($_POST['data'])) {
    echo(hash("sha256", $_POST['data']));
}
?></textarea>
<button type="submit">submit</button>
</form>