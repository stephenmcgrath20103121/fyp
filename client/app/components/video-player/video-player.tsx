"use client";
import * as React from 'react';
import Grid from '@mui/material/Grid';
import Hls from "hls.js";
import Plyr from "plyr";
import "plyr/dist/plyr.css";

type Props = {
  src: string;
  poster?: string;
  storageKey?: string;
  onPlay?: () => void;
  onEnded?: () => void;
};

const RESUME_PREFIX = "video-resume:";
const SAVE_INTERVAL_MS = 5000;
const MIN_SAVE_SECONDS = 5;
const END_THRESHOLD = 10;

export default function VideoPlayer({
  src,
  poster,
  storageKey,
  onPlay,
  onEnded,
}: Props) {
  const videoRef = React.useRef<HTMLVideoElement | null>(null);
  const hlsRef = React.useRef<Hls | null>(null);
  const playerRef = React.useRef<Plyr | null>(null);
  const resumeKey = RESUME_PREFIX + (storageKey ?? src);

  React.useEffect(() => {
    const video = videoRef.current;
    if (!video) return;

    const restorePosition = () => {
      try {
        const saved = localStorage.getItem(resumeKey);
        if (!saved) return;
        const t = parseFloat(saved);
        if (!Number.isFinite(t) || t < MIN_SAVE_SECONDS) return;
        if (video.duration && t > video.duration - END_THRESHOLD) return;
        video.currentTime = t;
      } catch {}
    };

    const savePosition = () => {
      try {
        const t = video.currentTime;
        if (t < MIN_SAVE_SECONDS) return;
        if (video.duration && t > video.duration - END_THRESHOLD) {
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
        "play-large", "play", "progress", "current-time", "duration",
        "mute", "volume", "settings", "pip", "airplay", "fullscreen",
      ],
      settings: ["quality", "speed"],
      keyboard: {focused: true, global: true },
      tooltips: { controls: true, seek: true },
      speed: { selected: 1, options: [0.25, 0.5, 0.75, 1, 1.5, 2, 4, 8, 16] },
    };

    // Safari - use native HLS
    if (video.canPlayType("application/vnd.apple.mpegurl")) {
      video.src = src;
      playerRef.current = new Plyr(video, basePlyrOptions);
    }
    else if (Hls.isSupported()) {
      const hls = new Hls({ enableWorker: true, lowLatencyMode: false });
      hls.loadSource(src);
      hls.attachMedia(video);
      hlsRef.current = hls;

      hls.on(Hls.Events.MANIFEST_PARSED, () => {
        const levels = hls.levels.map((l) => l.height).filter(Boolean);
        const qualityOptions = [0, ...levels];

        playerRef.current = new Plyr(video, {
          ...basePlyrOptions,
          quality: {
            default: 0,
            options: qualityOptions,
            forced: true,
            onChange: (newQuality: number) => {
              if (newQuality === 0) {
                hls.currentLevel = -1;
              } else {
                const idx = hls.levels.findIndex((l) => l.height === newQuality);
                if (idx !== -1) hls.currentLevel = idx;
              }
            },
          },
          i18n: { qualityLabel: { 0: "Auto" } },
        });
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
      video.src = src;
    }

    // Events
    const handleLoadedMetadata = () => restorePosition();
    const handlePlay = () => onPlay?.();
    const handlePause = () => savePosition();
    const handleEnded = () => { clearPosition(); onEnded?.(); };

    video.addEventListener("loadedmetadata", handleLoadedMetadata);
    video.addEventListener("play", handlePlay);
    video.addEventListener("pause", handlePause);
    video.addEventListener("ended", handleEnded);

    const saveTimer = window.setInterval(() => {
      if (!video.paused && !video.ended) savePosition();
    }, SAVE_INTERVAL_MS);

    const handleUnload = () => savePosition();
    window.addEventListener("beforeunload", handleUnload);

    // Cleanup
    return () => {
      savePosition();
      window.clearInterval(saveTimer);
      window.removeEventListener("beforeunload", handleUnload);
      video.removeEventListener("loadedmetadata", handleLoadedMetadata);
      video.removeEventListener("play", handlePlay);
      video.removeEventListener("pause", handlePause);
      video.removeEventListener("ended", handleEnded);
      playerRef.current?.destroy();
      hlsRef.current?.destroy();
      playerRef.current = null;
      hlsRef.current = null;
    };
  }, [src, resumeKey, onPlay, onEnded]);

  return (
    <Grid container sx={{justifyContent: 'center', bgcolor: 'primary.contrastText'}}>
      <video
        ref={videoRef}
        poster={poster}
        controls
        crossOrigin="anonymous"
        playsInline
        style={{ width: "100%" }}
      />
    </Grid>
  );
}