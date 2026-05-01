"use client";
import * as React from 'react';
import Image from 'next/image';
import Grid from '@mui/material/Grid';
import Typography from '@mui/material/Typography';
import { Media } from '@/app/types/index';


type Props = {
  src: string;
  media: Media | null;
  alt?: string;
};

export default function ImageViewer({ src, media, alt }: Props) {
  if (!media) {
    return (
      <Typography sx={{ color: 'error.main', p: 2 }}>
        Failed to load image metadata
      </Typography>
    );
  }

  const width  = media.width  > 0 ? media.width  : 1920;
  const height = media.height > 0 ? media.height : 1080;

  return (
    <Grid
      container
      sx={{
        justifyContent: 'center',
        alignItems: 'center',
        bgcolor: 'primary.contrastText',
      }}
    >
      <Image
        src={src}
        alt={alt ?? media.title}
        width={width}
        height={height}
        unoptimized
        style={{
          maxWidth: '100%',
          height: 'auto',
          objectFit: 'contain',
        }}
      />
    </Grid>
  );
}