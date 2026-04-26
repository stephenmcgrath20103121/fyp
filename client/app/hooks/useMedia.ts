import { useQuery } from '@tanstack/react-query';
import { getMedia } from '@/app/api/mediaroot-server-api';

const useMedia = () => {
  return useQuery({
    queryKey: ['media'],
    queryFn: () => getMedia(),
  })
}

export { useMedia }