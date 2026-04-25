import { useQuery } from '@tanstack/react-query';
import { Media } from '@/app/types/index';
import { getMedia } from '@/app/api/mediaroot-server-api';

const useMedia = () => {
  return useQuery({
    queryKey: ['media'],
    queryFn: () => getMedia(),
  })
}

export { useMedia }