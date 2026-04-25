import { Media } from '@/app/types/index';

export async function getMedia(): Promise<Media[]>{
    const results = await fetch (`${process.env.NEXT_PUBLIC_MEDIA_API}/api/media`);
    if(!results.ok) throw new Error('Failed to fetch your Media');
    return results.json();
  };
