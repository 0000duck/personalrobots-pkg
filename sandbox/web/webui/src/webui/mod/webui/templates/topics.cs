<html>
  <head>
    <?cs include:"includes.cs" ?>
</head>

<body onload="ros_handleOnLoad('/ros')">
<?cs include:"header.cs" ?>
<br>

<h3>Published topics:</h3>
<ul objtype="ListWidget" topic="/topics" key="pubtopics">
  <li><a href="topic.py?topic=__item__">__item__</a>
</ul>

<!--
<h3>Subscribed topics:</h3>
<ul objtype="ListWidget" topic="/topics" key="subtopics">
  <li>__item__
</ul>
-->

<br>

</body>
</html>
