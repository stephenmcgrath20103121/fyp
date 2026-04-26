import { useMutation, useQueryClient } from '@tanstack/react-query';
import { createMedia } from '@/app/api/mediaroot-server-api';
import { Media } from '@/app/types/index';

type CreateMediaArgs = { filePath: string; title?: string };

const useCreateMedia = () => {
  const queryClient = useQueryClient();
  return useMutation<Media, Error, CreateMediaArgs>({
    mutationFn: ({ filePath, title }) => createMedia(filePath, title),
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['media'] });   //refresh library after add
    },
  });
};

export { useCreateMedia };