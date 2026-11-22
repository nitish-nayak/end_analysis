# display

Builds the event display page from the JSON written by `analysis/EventDump.C`:

```
python3 display/make_event_display.py events.json event_display.html
```

`--user` and `--password` give a password-protected page. The page is a single HTML file and needs
internet access only to load three.js.
