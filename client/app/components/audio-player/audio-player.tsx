"use client";
import * as React from 'react';
import Grid from '@mui/material/Grid';
import Hls from "hls.js";
import Plyr from "plyr";
import "plyr/dist/plyr.css";

type Props = {
  src: string;
  storageKey?: string;
  onPlay?: () => void;
  onEnded?: () => void;
};

const RESUME_PREFIX = "audio-resume:";
const SAVE_INTERVAL_MS = 5000;
const MIN_SAVE_SECONDS = 5;
const END_THRESHOLD = 10;

export default function AudioPlayer({
  src,
  storageKey,
  onPlay,
  onEnded,
}: Props) {
  const audioRef  = React.useRef<HTMLAudioElement | null>(null);
  const hlsRef    = React.useRef<Hls | null>(null);
  const playerRef = React.useRef<Plyr | null>(null);
  const resumeKey = RESUME_PREFIX + (storageKey ?? src);

  React.useEffect(() => {
    const audio = audioRef.current;
    if (!audio) return;

    const restorePosition = () => {
      try {
        const saved = localStorage.getItem(resumeKey);
        if (!saved) return;
        const t = parseFloat(saved);
        if (!Number.isFinite(t) || t < MIN_SAVE_SECONDS) return;
        if (audio.duration && t > audio.duration - END_THRESHOLD) return;
        audio.currentTime = t;
      } catch {}
    };

    const savePosition = () => {
      try {
        const t = audio.currentTime;
        if (t < MIN_SAVE_SECONDS) return;
        if (audio.duration && t > audio.duration - END_THRESHOLD) {
          localStorage.removeItem(resumeKey);
          return;
        }
        localStorage.setItem(resumeKey, String(t));
      } catch {}
    };

    const clearPosition = () => {
      try { localStorage.removeItem(resumeKey); } catch {}
    };

    const basePlyrOptions: Plyr.Options = {
      controls: [
        "play", "progress", "current-time", "duration",
        "mute", "volume", "settings",
      ],
      settings: ["speed"],
      keyboard: { focused: true, global: true },
      tooltips: { controls: true, seek: true },
      speed: { selected: 1, options: [0.5, 0.75, 1, 1.25, 1.5, 2] },
    };

    // Safari - native HLS
    if (audio.canPlayType("application/vnd.apple.mpegurl")) {
      audio.src = src;
      playerRef.current = new Plyr(audio, basePlyrOptions);
    }
    else if (Hls.isSupported()) {
      const hls = new Hls({ enableWorker: true, lowLatencyMode: false });
      hls.loadSource(src);
      hls.attachMedia(audio);
      hlsRef.current = hls;

      hls.on(Hls.Events.MANIFEST_PARSED, () => {
        playerRef.current = new Plyr(audio, basePlyrOptions);
      });

      hls.on(Hls.Events.ERROR, (_evt, data) => {
        if (!data.fatal) return;
        switch (data.type) {
          case Hls.ErrorTypes.NETWORK_ERROR: hls.startLoad();         break;
          case Hls.ErrorTypes.MEDIA_ERROR:   hls.recoverMediaError(); break;
          default:                           hls.destroy();           break;
        }
      });
    } else {
      audio.src = src;
    }

    const handleLoadedMetadata = () => restorePosition();
    const handlePlay  = () => onPlay?.();
    const handlePause = () => savePosition();
    const handleEnded = () => { clearPosition(); onEnded?.(); };

    audio.addEventListener("loadedmetadata", handleLoadedMetadata);
    audio.addEventListener("play",  handlePlay);
    audio.addEventListener("pause", handlePause);
    audio.addEventListener("ended", handleEnded);

    const saveTimer = window.setInterval(() => {
      if (!audio.paused && !audio.ended) savePosition();
    }, SAVE_INTERVAL_MS);

    const handleUnload = () => savePosition();
    window.addEventListener("beforeunload", handleUnload);

    return () => {
      savePosition();
      window.clearInterval(saveTimer);
      window.removeEventListener("beforeunload", handleUnload);
      audio.removeEventListener("loadedmetadata", handleLoadedMetadata);
      audio.removeEventListener("play",  handlePlay);
      audio.removeEventListener("pause", handlePause);
      audio.removeEventListener("ended", handleEnded);
      playerRef.current?.destroy();
      hlsRef.current?.destroy();
      playerRef.current = null;
      hlsRef.current = null;
    };
  }, [src, resumeKey, onPlay, onEnded]);

  return (
    <Grid container sx={{ justifyContent: 'center', bgcolor: 'primary.contrastText', p: 2 }}>
      <audio
        ref={audioRef}
        controls
        crossOrigin="anonymous"
        style={{ width: "100%" }}
      />
    </Grid>
  );
}