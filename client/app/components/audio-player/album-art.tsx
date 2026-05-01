'use client';
import * as React from 'react';
import placeholder from '@/public/albumArtPlaceholder.svg';

type Props = {
  src: string;
  alt: string;
};

export default function AlbumArt({ src, alt }: Props) {
  return (
    <img
      src={src}
      onError={(e) => { (e.currentTarget as HTMLImageElement).src = placeholder.src; }}
      alt={alt}
      style={{ width: 200, height: 200, objectFit: 'cover' }}
    />
  );
}